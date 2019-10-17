---
date: Sat Oct 12 10:47:23 EDT 2019
author: Luiz Carlos Rumbelsperger Viana
---

## To get rax:

```bash
git clone https://github.com/antirez/rax.git

cd rax

#to get repo to version used in [date]
git reset --hard 3562565

rm -rf .git README.md TODO.md Makefile

rm rax-oom-test.c rax-test.c

cd ..

mkdir src

mv rax src/
```

---

## To get Unity test framework:

```bash
mkdir -p test/unity

git clone https://github.com/ThrowTheSwitch/Unity.git unity

cd unity

#to get repo to version used in [date]
git reset --hard 8ce41ed

mv src/* LICENSE.txt ../test/unity

cd ..

rm -rf unity

rm test/unity/meson.build
```

## To get premake5 (optional)

Just download tar from https://github.com/premake/premake-core/releases/download/v5.0.0-alpha14/premake-5.0.0-alpha14-linux.tar.gz and extract the executable somewhere in your PATH.

Check the COPYING or LICENSE files in the dependency folders for legal information.

These dependencies were commited to the project for convenience since they were released with very permissive licenses. So they should already be present in this repository.