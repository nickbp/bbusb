DIRS=src
CMD=

runcmd:
	for dir in $(DIRS); do (cd $$dir; $(MAKE) $(CMD)); done

# Print debug messages of what's being sent to the sign:
debug: CMD=debug 
debug: runcmd

# Create packets, but don't actually send them anywhere (implies debug):
nousb: CMD=nousb
nousb: runcmd

all: runcmd

clean: CMD=clean
clean: runcmd
