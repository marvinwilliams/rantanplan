#-----------------------------------------------------------------------#
#- GLOBAL DEFS ---------------------------------------------------------#
#-----------------------------------------------------------------------#

# Keep this as generic as possible.

NAME=glucose

# Avoid hardcoding the version number in this file.  This allows us to just
# drop a new distribution package into this directory without changing
# anything else. For other more complicated set-ups / solvers something like
# 'VERSION=960' should also work, if you have the appropriate tar file
# available, e.g., 'picosat-960.tar.gz', which extracts into 'picosat-960'.

# NOTE: requires GNU make extension '$(shell ...)'.

VERSION=4
#-----------------------------------------------------------------------#
# Solver signatures have to be both valid file names and C symbols.
# Since Picosat uses a dash '-' for the package name, we have to
# differentiate between with (directory / package name) and without
# dash (the signature).

BUILD_DIR ?= build
SIG=$(NAME)$(VERSION)
DIR=$(NAME)-$(VERSION)
TARGET=$(BUILD_DIR)/libipasir$(SIG).a

#-----------------------------------------------------------------------#

CXX=g++  

CXXFLAGS= -g -std=c++11 -Wall -DNDEBUG -O3

#-----------------------------------------------------------------------#
#- REQUIRED TOP RULES --------------------------------------------------#
#-----------------------------------------------------------------------#

all: $(TARGET)

clean:
	rm -f *.o *.a

#-----------------------------------------------------------------------#
#- INVISIBLE INTERNAL SUB RULES ----------------------------------------#
#-----------------------------------------------------------------------#

$(TARGET): .FORCE
	@#
	@# extract library
	@#
	@#
	@# configure and build library
	@#
	$(MAKE) -C $(DIR) lr
	@#
	@# compile glue code
	@#
	$(MAKE) $(BUILD_DIR)/ipasir$(NAME)glue.o
	@#
	@# merge library and glue code into target
	@#
	cp $(BUILD_DIR)/release/lib/lib$(NAME).a $(TARGET)
	ar r $(TARGET) $(BUILD_DIR)/ipasir$(NAME)glue.o

#-----------------------------------------------------------------------#
#- LOCAL GLUE RULES ----------------------------------------------------#
#-----------------------------------------------------------------------#

$(BUILD_DIR)/ipasir$(NAME)glue.o: ipasir$(NAME)glue.cc ipasir.h makefile
	$(CXX) -g  -std=c++11 $(CXXLAGS) \
	  -DVERSION=\"$(VERSION)\" \
	  -I$(DIR) -I$(DIR)/core -c ipasir$(NAME)glue.cc -o $(BUILD_DIR)/ipasir$(NAME)glue.o

#-----------------------------------------------------------------------#

.FORCE:
.PHONY: all clean
