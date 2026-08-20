#!/usr/bin/env bash
set -e
sudo apt-get update -qq
sudo apt-get install -y -qq clang g++ make python3 python3-pip xz-utils zip unzip curl \
  >/dev/null 2>&1 || true
pip3 install --quiet pycryptodome || true
echo "== deps ready =="
clang --version | head -1
python3 -c "import Crypto; print('pycryptodome ok')"
