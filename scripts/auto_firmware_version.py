import os
import subprocess
import datetime

Import("env")

def get_firmware_specifier_build_flag():
    # Fork builds: the CI workflow pre-computes out/version.txt with a
    # patch-bumped "-fork." prerelease so the semver compares newer than
    # upstream. Reuse it so BUILD_GIT_VERSION matches the release's
    # version.txt exactly (otherwise isUpdateAvailable flips on every boot).
    version_file = "out/version.txt"
    if os.path.exists(version_file):
        with open(version_file) as f:
            build_version = f.read().strip()
    else:
        ret = subprocess.run(["git", "describe", "--tags", "--dirty", "--exclude", "nightly"], stdout=subprocess.PIPE, text=True) #Uses any tags
        build_version = ret.stdout.strip()
    build_flag = "#define BUILD_GIT_VERSION \"" + build_version + "\""
    print ("Build version: " + build_version)
    return build_flag

def get_time_specifier_build_flag():
    build_timestamp = datetime.datetime.now(datetime.UTC).strftime("%Y-%m-%dT%H:%M:%SZ")
    build_flag = "#define BUILD_TIMESTAMP \"" + build_timestamp + "\""
    print ("Build date: " + build_timestamp)
    return build_flag

with open('src/version.h', 'w') as f:
    f.write(
        '#pragma once\n' +
        '#ifndef GIT_VERSION_H\n' +
        '#define GIT_VERSION_H\n' +
        get_firmware_specifier_build_flag() + '\n' +
        get_time_specifier_build_flag() + '\n'
        '#endif\n'
    )
