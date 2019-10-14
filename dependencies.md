---
date: Sat Oct 12 10:47:23 EDT 2019
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

Check the COPYING or LICENSE files in the depency folders for legal information.