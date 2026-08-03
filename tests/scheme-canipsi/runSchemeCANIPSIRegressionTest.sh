#!/usr/bin/env bash

set -euo pipefail

repositoryDirectory="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "${repositoryDirectory}"

if [[ -x /usr/local/opt/openjdk/bin/java ]];
then
	export JAVA_HOME="/usr/local/opt/openjdk/libexec/openjdk.jdk/Contents/Home"
	export PATH="${JAVA_HOME}/bin:${PATH}"
elif [[ -x /opt/homebrew/opt/openjdk/bin/java ]];
then
	export JAVA_HOME="/opt/homebrew/opt/openjdk/libexec/openjdk.jdk/Contents/Home"
	export PATH="${JAVA_HOME}/bin:${PATH}"
fi

temporaryDirectory="$(mktemp -d)"
dependencies="$(find lib -maxdepth 1 -type f -name '*.jar' -print | LC_ALL=C sort | paste -sd: -)"
test -n "${dependencies}"

javac -Xlint:all -cp "${dependencies}" -d "${temporaryDirectory}" \
	SchemeCANIFPPCT/SchemeCANIPSI.java tests/java/SchemeCANIPSITest.java
java -cp "${temporaryDirectory}:${dependencies}" SchemeCANIPSITest
