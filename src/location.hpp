#ifndef LOCATION_HPP
#define LOCATION_HPP

struct Position {
	int byte_offset {0};
	int line {0};
	int column {0};
};

struct Location {
	Position begin {};
	Position end {};
};

#endif
