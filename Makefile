# Build the example.
all: typestring.hh
	$(CXX) -I . -std=c++11 -Wall -Wextra -pedantic example.cc -o example

# Build and run the example.
run: all
	./example

# Download the typestring.hh header.
typestring.hh:
	wget -q https://github.com/irrequietus/typestring/raw/master/typestring.hh
	touch $@

# Remove the example.
clean:
	rm -rf example