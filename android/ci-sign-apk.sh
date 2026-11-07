#!/usr/bin/env bash
# Check snapshot signing configuration or sign an already aligned APK.
set -euo pipefail

mode=${1:-}
if [[ "$mode" != check && "$mode" != sign ]]; then
    echo "Usage: $0 check | sign INPUT_APK OUTPUT_APK" >&2
    exit 1
fi

if [[ -z "${ANDROID_SIGNING_KEY:-}${ANDROID_KEY_ALIAS:-}${ANDROID_KEYSTORE_PASSWORD:-}${ANDROID_KEY_PASSWORD:-}" ]]; then
    if [[ "$mode" == check && "${GITHUB_REPOSITORY:?}" != exult/exult ]]; then
        echo "::notice::No Android signing secrets configured; keeping debug and unsigned release APKs."
        echo "enabled=false" >> "${GITHUB_OUTPUT:?}"
        exit 0
    fi
fi

for name in ANDROID_SIGNING_KEY ANDROID_KEY_ALIAS ANDROID_KEYSTORE_PASSWORD; do
    if [[ -z "${!name:-}" ]]; then
        echo "::error::Missing required Android signing secret: $name"
        exit 1
    fi
done

umask 077
keystore=$(mktemp "${RUNNER_TEMP:?}/exult-keystore.XXXXXX")
signed=
trap 'rm -f -- "$keystore"; if [[ -n "$signed" ]]; then rm -f -- "$signed" "$signed.idsig"; fi' EXIT
if ! printf '%s' "$ANDROID_SIGNING_KEY" | base64 --decode > "$keystore" || [[ ! -s "$keystore" ]]; then
    echo "::error::ANDROID_SIGNING_KEY must contain a nonempty base64-encoded keystore."
    exit 1
fi

export ANDROID_KEY_PASSWORD="${ANDROID_KEY_PASSWORD:-$ANDROID_KEYSTORE_PASSWORD}"
if [[ "$mode" == check ]]; then
    # Generate and discard a CSR to check both the store and private-key passwords.
    if ! keytool -certreq -keystore "$keystore" -alias "$ANDROID_KEY_ALIAS" \
        -storepass:env ANDROID_KEYSTORE_PASSWORD -keypass:env ANDROID_KEY_PASSWORD > /dev/null 2>&1; then
        echo "::error::Cannot unlock the Android signing key; check the keystore, alias, and both passwords."
        exit 1
    fi
    echo "enabled=true" >> "${GITHUB_OUTPUT:?}"
else
    if [[ $# != 3 ]]; then
        echo "Usage: $0 sign INPUT_APK OUTPUT_APK" >&2
        exit 1
    fi
    apksigner="${ANDROID_SDK_ROOT:?}/build-tools/34.0.0/apksigner"
    signed=$(mktemp "${RUNNER_TEMP:?}/exult-signed.XXXXXX.apk")
    "$apksigner" sign --ks "$keystore" --ks-key-alias "$ANDROID_KEY_ALIAS" \
        --ks-pass env:ANDROID_KEYSTORE_PASSWORD --key-pass env:ANDROID_KEY_PASSWORD \
        --out "$signed" "$2"
    "$apksigner" verify "$signed"
    cp -- "$signed" "$3"
fi
