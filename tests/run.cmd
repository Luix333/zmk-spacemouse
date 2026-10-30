@echo off
rem Host unit tests for the pure-C engine, compiled with gcc in WSL (Windows has no C compiler here).
rem Usage: tests\run.cmd   (exit code 0 = all tests passed)
wsl -d Ubuntu-22.04 --cd "%~dp0.." -- bash -c "mkdir -p build && gcc -std=c11 -Wall -Wextra -Werror -Iinclude src/sm_engine.c tests/test_engine.c -o build/test_engine && ./build/test_engine"
