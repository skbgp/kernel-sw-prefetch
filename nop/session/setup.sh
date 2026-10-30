#!/bin/bash
set -euxo pipefail
mkdir -p "$HOME/nop-records"
exec > >(tee -a "$HOME/nop-records/setup.log") 2>&1
sudo apt-get update
sudo apt-get install -y build-essential flex bison bc libssl-dev libelf-dev cpio qemu-system-x86 busybox-static git python3
cd "$HOME"
git clone https://github.com/skbgp/kernel-sw-prefetch.git kernel-sw-prefetch
cd kernel-sw-prefetch
git remote add upstream https://github.com/bpf-endeavor/kernel-sw-prefetch.git
git fetch upstream main
git switch nop-verifier
git status --short --branch
git log -4 --oneline
git worktree add --detach ../kernel-original 680b721a3232f6d689a67c1ca37c6cf35fe1df6e
