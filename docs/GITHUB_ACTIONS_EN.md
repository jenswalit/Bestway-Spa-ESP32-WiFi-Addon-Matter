# Building firmware on GitHub

**English** | [Nederlands](GITHUB_ACTIONS_NL.md)

GitHub Actions can build this firmware on a Linux runner. You do not need to install ESP-IDF and ESP-Matter on your own computer to use this workflow.

## Recorded baseline

- Runner: Ubuntu 22.04.
- ESP-IDF: v5.5.5, matching the supplied project sdkconfig header and dependencies.lock.
- ESP-Matter: `ff9f07ecb0bf01d7a2db5428149ef16bf601774c`.
- connectedhomeip: `539342f32d5f4dc93761c2f9325afe29270068f1`.
- Target: ESP32-C6 with the existing `firmware/sdkconfig` and partitions.

The workflow verifies the exact CHIP submodule revision and stops if it differs. It installs the ESP32-C6 IDF tools and skips building Matter host tools. Firmware sources are not modified.

## Install from your existing local repository

Download `Bestway_GitHub_Actions_ESP32C6_MoblyFix.zip` to Downloads. Run these commands from your local repository, stopping on any error:

```bash
git pull --ff-only
unzip -o ~/Downloads/Bestway_GitHub_Actions_ESP32C6_MoblyFix.zip -d .
git add -- .github/workflows/build-esp32c6.yml docs/GITHUB_ACTIONS_NL.md docs/GITHUB_ACTIONS_EN.md
git diff --cached --stat
git commit -m "Add GitHub Actions ESP-IDF and ESP-Matter firmware build"
git push origin main
```

## Python setup: mobly conflict

The first workflow used ESP-Matter `install.sh --no-host-tool`. This skips host tools but still installs the full CHIP test environment through `bootstrap.sh -p all,esp32`, including `mobly`.

This workflow first runs `bootstrap.sh -p esp32`, then `install.sh --no-host-tool --no-bootstrap`. The base CHIP build environment (GN/ZAP and core requirements) remains available; additional test dependencies from `requirements.all.txt` are not installed. ESP-IDF, ESP-Matter and CHIP revisions are unchanged. No dependency resolver is bypassed and firmware sources are unchanged.

The supplied error excerpt does not identify the specific conflicting dependencies. This change is therefore not proof of a successful full build. Check the new GitHub run. If it fails, share the log starting at the first error, including `The conflict is caused by` lines when present.

## Start a build

1. Open the GitHub repository.
2. Select **Actions** and enable workflows if prompted.
3. Select **Build ESP32-C6 firmware**.
4. Select **Run workflow → main → Run workflow**.
5. Open the run to see progress and errors.

The initial workflow push also triggers a build. Subsequent changes on `main` to firmware, tests, `tools/verify.sh` or this workflow trigger builds automatically. Documentation-only and hardware changes do not trigger firmware compilation.

The initial build downloads SDKs and dependencies and may take a while. The run timeout is 90 minutes. On failure, open the failed step and share its first relevant error. Do not change SDK versions at random.

## Download the firmware

After a successful run, find **Artifacts** at the bottom of the run page and download `Bestway_ESP32C6_OTA_<run number>`. Artifact downloads generally require signing in to GitHub. Retention is 30 days, or less if limited by repository policy.

Extract the archive. It contains:

- `Bestway_Matter_v5.bin`: OTA application image.
- `SHA256SUMS.txt`: checksum.
- `build-info.json`: source revision, SDK revisions and image information.
- `README.txt`: brief usage instructions.

Use the module's existing OTA page and select **the extracted .bin file**, not the ZIP. Keep power and WiFi available. Normal OTA updates require neither factory reset nor NVS erasure.

This artifact is not a complete USB installation package for a blank ESP. See [build instructions](BUILD_EN.md) for initial USB installation. A successful build demonstrates compilation, not a physical spa test.

## Validation and publication status

Workflow structure and shell syntax were checked locally. A full ESP-IDF/Matter build has not been performed in this environment. The first GitHub run must confirm SDK installation and full compilation.

The workflow has read-only repository permissions. It neither publishes a GitHub Release nor performs automatic OTA. After a successful build and hardware test, the tested image can be attached to a Release for longer-term public availability.

References: [GitHub manual workflow runs](https://docs.github.com/en/actions/how-tos/manage-workflow-runs/manually-run-a-workflow), [artifact upload](https://github.com/actions/upload-artifact), [Espressif Matter setup](https://docs.espressif.com/projects/esp-matter/en/release-v1.4.2/esp32c6/developing.html).
