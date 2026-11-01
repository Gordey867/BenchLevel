CC = gcc
SOURCES = test.c test_lab3.c ../firmware/Devices/mpu6050.c ../firmware/App/level.c ../firmware/App/controller.c ../firmware/App/calibration_store.c ../firmware/App/console.c ../firmware/Platform/format.c ../firmware/Platform/clock_track.c
HEADERS = $(wildcard ../firmware/Devices/*.h ../firmware/App/*.h) ../firmware/Platform/clock_track.h ../firmware/Platform/format.h captured_vector.h captured_tilt.h
CFLAGS = -std=c11 -Wall -Wextra -Werror -g -O0 --coverage -I../firmware/Devices -I../firmware/App -I../firmware/Platform
.PHONY: test coverage coverage-lab3 sanitize clean
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

coverage-lab3: build/benchlevel-tests
	rm -f build/*.gcda
	./build/benchlevel-tests
	gcovr --root .. --filter '../firmware/(App|Devices)/' --txt ../docs/metrics/lab3/coverage.txt --json-summary ../docs/metrics/lab3/coverage.json --fail-under-line 90

sanitize:
	mkdir -p build
	$(CC) -std=c11 -Wall -Wextra -Werror -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -I../firmware/Devices -I../firmware/App -I../firmware/Platform $(SOURCES) -lm -o build/benchlevel-sanitize
	ASAN_OPTIONS=detect_leaks=1 ./build/benchlevel-sanitize

.PHONY: candidate-test
candidate-test:
	mkdir -p build
	$(CC) -std=c11 -Wall -Wextra -Werror -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -I../firmware/App -I../firmware/Platform test_candidate.c ../firmware/App/console.c -o build/benchlevel-candidate-tests
	./build/benchlevel-candidate-tests
