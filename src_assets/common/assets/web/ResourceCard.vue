<template>
    <footer class="text-center text-body-secondary opacity-75 mt-5 mb-3 px-3" style="font-size: .75rem">
        <p class="mb-1">{{ $t('resource_card.legal_notice') }}</p>
        <p class="mb-0">
            <a class="link-secondary" href="https://github.com/ycoooding/lumen/tree/v1.0.0" target="_blank" rel="noopener noreferrer">{{ $t('resource_card.source_code') }}</a>
            <span class="mx-2">·</span>
            <a class="link-secondary" href="https://github.com/ycoooding/lumen/blob/v1.0.0/LICENSE" target="_blank" rel="noopener noreferrer">{{ $t('resource_card.license') }}</a>
            <span class="mx-2">·</span>
            <a class="link-secondary" href="https://github.com/ycoooding/lumen/blob/v1.0.0/NOTICE" target="_blank" rel="noopener noreferrer">{{ $t('resource_card.third_party_notice') }}</a>
            <span class="mx-2">·</span>
            <a class="link-secondary" href="https://github.com/LizardByte/Sunshine/tree/v2025.924.154138" target="_blank" rel="noopener noreferrer">{{ $t('resource_card.upstream') }}</a>
        </p>
    </footer>
</template>
