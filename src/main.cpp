#include <utility>
#include <tuple>
#include <iostream>

using namespace std;

void print(int i, ostream & os) {
	os << i;
}

void print(string s, ostream & os) {
	os << s;
}

template <class ... Elems> void print(tuple<Elems...> t, ostream & os);

template <class Tuple>
void print_tuple_impl(Tuple tuple, ostream & os, integer_sequence<size_t>){}

template <class Tuple, size_t ... Indices>
void print_tuple_impl(Tuple tuple, ostream & os, integer_sequence<size_t, 0, Indices...>) {
	print(get<0>(tuple), os);
	int phony[] = {(os << ",", print(get<Indices>(tuple), os), 0)...};
}

template <class ... Elems>
void print(tuple<Elems...> t, ostream & os) {
	os << "[";
	print_tuple_impl(t, os, index_sequence_for<Elems...>{});
	os << "]";
}

int main() {
	print(make_tuple(1, "abc"s, make_tuple(1, "2"s , 3)), cout);
}
