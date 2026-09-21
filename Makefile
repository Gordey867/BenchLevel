IMAGE ?= benchlevel-build:local

.PHONY: image build clean flash monitor

image:
	docker build -t $(IMAGE) docker

build: image
	docker run --rm -v "$(CURDIR):/workspace" -w /workspace \
		--user "$(shell id -u):$(shell id -g)" $(IMAGE) \
		make -C firmware all

clean:
	$(MAKE) -C firmware clean

flash monitor:
	@echo 'VS Code: F1 -> Wokwi: Start Simulator. UART opens inside the simulator.'
