// Exercise the actual config page lifecycle/save methods and Checkbox value mapping.
// No running Lumen service or user configuration is touched.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
import { fileURLToPath } from 'node:url';

const root = fileURLToPath(new URL('../../', import.meta.url));
const web = root + 'src_assets/common/assets/web/';
const page = fs.readFileSync(web + 'config.html', 'utf8');
const script = page.match(/<script type="module">([\s\S]*?)<\/script>/)[1]
  .replace(/^\s*import .*$/gm, '');
const checkbox = fs.readFileSync(web + 'Checkbox.vue', 'utf8')
  .match(/<script setup>([\s\S]*?)<\/script>/)[1];
let definition;
let stored = {};
let lastPost;
const context = {
  createApp(options) { definition = options; return {}; },
  initApp() {}, computed: fn => fn, console,
  Navbar: {}, General: {}, Inputs: {}, Network: {}, Files: {}, Advanced: {},
  AudioVideo: {}, ContainerEncoders: {}, $tp() {}, usePlatformI18n() {},
  document: { getElementById() { return {}; } },
  async fetch(url, options) {
    assert.equal(url, './api/config');
    if (options?.method === 'POST') {
      lastPost = JSON.parse(options.body);
      assert.ok(Object.hasOwn(lastPost, 'udp_segmentation_offload'));
      // Match saveConfig's string/non-string conversion and parse_config's string response.
      stored = Object.fromEntries(Object.entries(lastPost)
        .filter(([, v]) => v !== null && v !== '')
        .map(([k, v]) => [k, typeof v === 'string' ? v : JSON.stringify(v)]));
      return {status: 200};
    }
    return {json: async () => ({platform: 'windows', status: true, version: '1.1.1', ...stored})};
  }
};
vm.runInNewContext(script, context);
async function refresh() {
  const app = definition.data();
  for (const [key, method] of Object.entries(definition.methods)) app[key] = method.bind(app);
  definition.created.call(app);
  await new Promise(resolve => setImmediate(resolve));
  return app;
}
function values(model) {
  const checkboxContext = {
    defineModel: () => ({value: model}), defineSlots: () => ({}),
    defineProps: () => ({class: '', id: 'udp_segmentation_offload', localePrefix: 'config', default: false}),
    console
  };
  return vm.runInNewContext(checkbox + '\n({checkboxValues, parsedDefaultPropValue, labelField, descField})', checkboxContext);
}

let app = await refresh();
assert.equal(app.config.udp_segmentation_offload, false, 'old config defaults to false');
assert.equal(values(app.config.udp_segmentation_offload).checkboxValues.falsy, false);
await app.save();
assert.equal(lastPost.udp_segmentation_offload, false, 'boolean false must be posted');
assert.equal(stored.udp_segmentation_offload, 'false');

app = await refresh();
assert.equal(app.config.udp_segmentation_offload, 'false');
app.config.udp_segmentation_offload = values(app.config.udp_segmentation_offload).checkboxValues.truthy;
await app.save();
assert.equal(stored.udp_segmentation_offload, 'true');

app = await refresh();
assert.equal(app.config.udp_segmentation_offload, 'true');
app.config.udp_segmentation_offload = values(app.config.udp_segmentation_offload).checkboxValues.falsy;
await app.save();
assert.equal(stored.udp_segmentation_offload, 'false');
assert.equal((await refresh()).config.udp_segmentation_offload, 'false');

for (const value of [true, false, 'true', 'false']) {
  stored = {udp_segmentation_offload: value};
  app = await refresh();
  await app.save();
  assert.equal(stored.udp_segmentation_offload, String(value));
}
const network = fs.readFileSync(web + 'configs/tabs/Network.vue', 'utf8');
assert.match(network, /v-model="config\.udp_segmentation_offload"/);
for (const locale of ['en', 'zh', 'zh_TW']) {
  const config = JSON.parse(fs.readFileSync(web + `public/assets/locale/${locale}.json`, 'utf8')).config;
  assert.ok(config.udp_segmentation_offload);
  assert.ok(config.udp_segmentation_offload_desc);
}
assert.match(fs.readFileSync(web + 'locale.js', 'utf8'), /fallbackLocale:\s*'en'/);
assert.match(fs.readFileSync(root + 'src/confighttp.cpp', 'utf8'), /v\.is_null\(\) \|\| \(v\.is_string\(\) && v\.get<std::string>\(\)\.empty\(\)\)/);
assert.match(fs.readFileSync(root + 'src/confighttp.cpp', 'utf8'), /v\.is_string\(\) \? v\.get<std::string>\(\) : v\.dump\(\)/);
console.log('PASS: missing/default false, enable, disable, refresh, boolean/string false, checkbox binding and locale fallback');
