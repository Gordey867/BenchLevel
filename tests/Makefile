CC = gcc
SOURCES = test.c ../firmware/Devices/mpu6050.c ../firmware/App/level.c ../firmware/App/controller.c
HEADERS = $(wildcard ../firmware/Devices/*.h ../firmware/App/*.h) captured_vector.h captured_tilt.h
CFLAGS = -std=c11 -Wall -Wextra -Werror -g -O0 --coverage -I../firmware/Devices -I../firmware/App
.PHONY: test coverage clean
test: build/benchlevel-tests
	./build/benchlevel-tests
build/benchlevel-tests: $(SOURCES) $(HEADERS) Makefile
	mkdir -p build
	rm -f build/*.gcda build/*.gcno
	$(CC) $(CFLAGS) $(SOURCES) -lm -o $@
coverage: build/benchlevel-tests
	rm -f build/*.gcda
	./build/benchlevel-tests
	mkdir -p ../docs/verification/lab2/coverage
	gcovr --root .. --filter '../firmware/(App|Devices)/' --txt ../docs/verification/lab2/coverage/summary.txt --json-summary ../docs/verification/lab2/coverage/summary.json --html-details ../docs/verification/lab2/coverage/index.html --fail-under-line 90
	cat ../docs/verification/lab2/coverage/summary.txt
clean:
	rm -rf build
