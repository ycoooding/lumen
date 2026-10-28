/**
 * @file tests/unit/platform/test_windows_uso.cpp
 * @brief USO policy and the unchanged stream.cpp fallback, with a test-only WinSock seam.
 */
#ifdef LUMEN_TEST_WRAP_SENDMSG
  #include "../../tests_common.h"
  #include "src/config.h"

  #include <array>
  #include <atomic>
  #include <boost/asio/ip/udp.hpp>
  #include <mutex>
  #include <thread>
  #include <winsock2.h>
  #include <ws2tcpip.h>

namespace config {
  void bool_f(std::unordered_map<std::string, std::string> &vars, const std::string &name, bool &input);
}

extern "C" int WSAAPI __real_WSASendMsg(SOCKET, LPWSAMSG, DWORD, LPDWORD, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE);

namespace {
  struct sent_message {
    std::string bytes;
    DWORD segment_size = 0;
  };

  std::mutex sent_mutex;
  std::vector<sent_message> sent;
  bool intercept = false;
  bool fail_batch = false;
}  // namespace

extern "C" int WSAAPI __wrap_WSASendMsg(SOCKET socket, LPWSAMSG msg, DWORD flags, LPDWORD bytes, LPWSAOVERLAPPED overlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE completion) {
  if (!intercept) {
    return __real_WSASendMsg(socket, msg, flags, bytes, overlapped, completion);
  }
  sent_message captured;
  for (DWORD i = 0; i < msg->dwBufferCount; ++i) {
    captured.bytes.append(msg->lpBuffers[i].buf, msg->lpBuffers[i].len);
  }
  for (auto control = WSA_CMSG_FIRSTHDR(msg); control; control = WSA_CMSG_NXTHDR(msg, control)) {
    if (control->cmsg_level == IPPROTO_UDP && control->cmsg_type == 2 /* UDP_SEND_MSG_SIZE */) {
      memcpy(&captured.segment_size, WSA_CMSG_DATA(control), sizeof(DWORD));
    }
  }
  const bool fail = fail_batch && captured.segment_size != 0;
  *bytes = fail ? 0 : static_cast<DWORD>(captured.bytes.size());
  std::lock_guard lock {sent_mutex};
  sent.push_back(std::move(captured));
  return fail ? SOCKET_ERROR : 0;
}

class WindowsUsoTest: public testing::Test {
protected:
  bool original_uso = config::stream.udp_segmentation_offload;
  std::array<char, 8> headers {'A', '0', 'B', '1', 'C', '2', 'D', '3'};
  std::array<char, 16> payload {'a', 'a', 'a', 'a', 'b', 'b', 'b', 'b', 'c', 'c', 'c', 'c', 'd', 'd', 'd', 'd'};
  std::vector<platf::buffer_descriptor_t> buffers {{payload.data(), 8}, {payload.data() + 8, 8}};
  boost::asio::ip::address peer_address = boost::asio::ip::make_address("127.0.0.1");

  struct session_data {
    struct {
      boost::asio::ip::udp::endpoint peer {boost::asio::ip::make_address("127.0.0.1"), 50000};
    } video;

    boost::asio::ip::address localAddress = boost::asio::ip::make_address("127.0.0.1");
  } session_value;

  session_data *session = &session_value;

  struct {
    SOCKET native_handle() const {
      return INVALID_SOCKET;
    }
  } sock;

  struct shard_view {
    char *headers;
    char *payload;
    size_t prefixsize;
    size_t blocksize = 4;

    const char *prefix(size_t i) const {
      return prefixsize ? headers + i * prefixsize : nullptr;
    }

    const char *data(size_t i) const {
      return payload + i * blocksize;
    }
  } shards {headers.data(), payload.data(), 2};

  size_t next_shard_to_send = 1;
  size_t current_batch_size = 3;

  platf::batched_send_info_t batch() {
    return {shards.prefixsize ? headers.data() : nullptr, shards.prefixsize, buffers, 4, next_shard_to_send, current_batch_size, static_cast<uintptr_t>(INVALID_SOCKET), peer_address, session->video.peer.port(), session->localAddress};
  }

  void send_via_stream_fallback() {
    auto batch_info = batch();
    using namespace std::literals;
    // Exact source block extracted by CMake, not a reimplementation of the fallback.
  #include "stream_batch_fallback.inc"
  }

  void SetUp() override {
    sent.clear();
    fail_batch = false;
    intercept = true;
    config::stream.udp_segmentation_offload = false;
  }

  void TearDown() override {
    intercept = false;
    config::stream.udp_segmentation_offload = original_uso;
  }

  void check_individual_packets() {
    ASSERT_EQ(sent.size(), 3);
    for (size_t i = 0; i < 3; ++i) {
      const auto index = next_shard_to_send + i;
      std::string expected;
      if (shards.prefixsize) {
        expected.append(shards.prefix(index), shards.prefixsize);
      }
      expected.append(shards.data(index), shards.blocksize);
      EXPECT_EQ(sent[i].bytes, expected);
      EXPECT_EQ(sent[i].segment_size, 0);
    }
  }
};

TEST_F(WindowsUsoTest, DefaultAndConfigRoundTrip) {
  EXPECT_FALSE(original_uso);
  bool enabled = config::stream_t {}.udp_segmentation_offload;
  for (const auto text : {"", "udp_segmentation_offload = true", "udp_segmentation_offload = false"}) {
    auto vars = config::parse_config(text);
    config::bool_f(vars, "udp_segmentation_offload", enabled);
    EXPECT_EQ(enabled, std::string_view(text).ends_with("true"));
  }
}

TEST_F(WindowsUsoTest, DisabledBatchDoesNotCallWinSock) {
  auto info = batch();
  EXPECT_FALSE(platf::send_batch(info));
  EXPECT_TRUE(sent.empty());
}

TEST_F(WindowsUsoTest, DisabledFallbackSendsEachHeaderAndPayloadExactlyOnce) {
  send_via_stream_fallback();
  check_individual_packets();
}

TEST_F(WindowsUsoTest, DisabledFallbackWithoutHeadersSendsEachFragmentExactlyOnce) {
  shards.prefixsize = 0;
  send_via_stream_fallback();
  check_individual_packets();
}

TEST_F(WindowsUsoTest, EnabledUsesOriginalSegmentedMessageThenCanBeDisabledAgain) {
  config::stream.udp_segmentation_offload = true;
  send_via_stream_fallback();
  ASSERT_EQ(sent.size(), 1);
  EXPECT_EQ(sent[0].segment_size, 6);
  EXPECT_EQ(sent[0].bytes, "B1bbbbC2ccccD3dddd");
  sent.clear();
  config::stream.udp_segmentation_offload = false;
  send_via_stream_fallback();
  check_individual_packets();
}

TEST_F(WindowsUsoTest, EnabledWithoutHeadersRetainsMultiplePayloadBuffers) {
  config::stream.udp_segmentation_offload = true;
  shards.prefixsize = 0;
  send_via_stream_fallback();
  ASSERT_EQ(sent.size(), 1);
  EXPECT_EQ(sent[0].segment_size, 4);
  EXPECT_EQ(sent[0].bytes, "bbbbccccdddd");
}

TEST_F(WindowsUsoTest, SinglePacketBehaviorIsIdenticalForBothSettings) {
  current_batch_size = 1;
  for (const bool enabled : {false, true}) {
    sent.clear();
    config::stream.udp_segmentation_offload = enabled;
    send_via_stream_fallback();
    ASSERT_EQ(sent.size(), 1);
    EXPECT_EQ(sent[0].segment_size, 0);
    EXPECT_EQ(sent[0].bytes, "B1bbbb");
  }
}

TEST_F(WindowsUsoTest, ExistingFailureFallbackStillWorksWhenEnabled) {
  config::stream.udp_segmentation_offload = true;
  fail_batch = true;
  send_via_stream_fallback();
  ASSERT_EQ(sent.size(), 4);
  EXPECT_EQ(sent.front().segment_size, 6);
  sent.erase(sent.begin());
  check_individual_packets();
}

TEST_F(WindowsUsoTest, ConcurrentDisabledBatchesNeverSend) {
  std::atomic<int> rejected {0};
  std::vector<std::thread> workers;
  for (int i = 0; i < 16; ++i) {
    workers.emplace_back([&] {
      auto info = batch();
      for (int attempt = 0; attempt < 100; ++attempt) {
        if (!platf::send_batch(info)) {
          ++rejected;
        }
      }
    });
  }
  for (auto &worker : workers) {
    worker.join();
  }
  EXPECT_EQ(rejected, 1600);
  EXPECT_TRUE(sent.empty());
}
#endif
