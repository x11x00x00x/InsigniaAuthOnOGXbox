# Insignia device auth — root build: delegates to native/ (NXDK Xbox binaries).
.PHONY: all native clean
all native:
	$(MAKE) -C native $(MAKEFLAGS)

clean:
	$(MAKE) -C native clean $(MAKEFLAGS)
