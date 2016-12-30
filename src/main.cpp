#include <utility>
#include <tuple>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <map>
#include <memory>

using erase_ptr = std::unique_ptr<void, void(*)(void *)>;
template <class T> void erased_deleter(void * t) {delete (T*)t;}

template <class T, class ...ARGS>
erase_ptr make_erased_ptr(ARGS && ... args) {
    return erase_ptr(new T(std::forward<ARGS>(args)...), erased_deleter<T>);
}

using namespace std;

template<class ... ARGS> struct type_list{};

struct RTProxy {
	string name;
	vector<RTProxy> args;
	bool operator==(const RTProxy & other ) const {return name == other.name && args == other.args;}
};

struct RTValue {
	RTProxy type;
	erase_ptr value;
};

template<class T> struct Proxy;
template<class T> Proxy<T> proxy;

#define REGISTER_TYPE(type) \
template<> \
struct Proxy<type> { \
	constexpr static const char * name = #type; \
	using args = type_list<>; \
}; \
RTValue toRuntime(const type & val) { return {toRuntime(proxy<type>), make_erased_ptr<type>(val)};}

#define REGISTER_TYPECTR(type_ctr) \
template<class ... ARGS> \
struct Proxy<type_ctr<ARGS...>> { \
	constexpr static const char * name = #type_ctr; \
	using args = type_list<ARGS...>; \
};

template<class T> RTProxy toRuntime(Proxy<T>);

template <class ... ARGS>
vector<RTProxy> toRuntime(type_list<ARGS...>) {
	return {toRuntime(proxy<ARGS>)...};
}

template<class T>
RTProxy toRuntime(Proxy<T>) {
	return {Proxy<T>::name, toRuntime(typename Proxy<T>::args{})};
}

struct unit {};

REGISTER_TYPE(unit)
REGISTER_TYPE(int)
REGISTER_TYPE(string)
REGISTER_TYPECTR(tuple)

struct runtime_tuple : vector<RTValue> {
};

RTValue toRuntime(runtime_tuple && rtup) {
	RTProxy result_type {"tuple"};
	for(auto & val: rtup) result_type.args.push_back(val.type);
	return RTValue{result_type, make_erased_ptr<runtime_tuple>(move(rtup))};
}

template <class ... Elems>
RTValue toRuntime(const tuple<Elems...> & tup);

template <class Tuple, size_t ... Indices>
RTValue tupToRuntimeImpl(const Tuple & tup, index_sequence<Indices...>) {
	runtime_tuple result;
	int phony[] = {(result.emplace_back(toRuntime(get<Indices>(tup))), 0)...};
	return toRuntime(move(result));
}

template <class ... Elems>
RTValue toRuntime(const tuple<Elems...> & tup) {
	return tupToRuntimeImpl(tup, index_sequence_for<Elems...>{});
}

using print_func = void(*)(const RTValue &, ostream &);

map<string, print_func> printers;

void print(const RTValue & value, ostream & os) {
	printers[value.type.name](value, os);
}

void print(int i, ostream & os) {
	os << i;
}

bool phony1 = []{
	printers["int"] = [](const RTValue & value, ostream & os) {print(* (int *) value.value.get(), os);};
	return true;
}();

void print(string s, ostream & os) {
	os << quoted(s);
}

bool phony2 = []{
	printers["string"] = [](const RTValue & value, ostream & os) {print(* (string *) value.value.get(), os);};
	return true;
}();

template <class Visitor, class Tuple, size_t ... Indices>
auto tuple_for_each_impl(const Tuple & t, Visitor v, index_sequence<Indices...>) {
	return tuple<decltype(v(get<Indices>(t)))...>{v(get<Indices>(t))...};
}

template <class Visitor, class ... Elems>
auto tuple_for_each(const tuple<Elems...> & t, Visitor v) {
	return tuple_for_each_impl(t, v, index_sequence_for<Elems...>{});
}

template<class Visitor>
RTValue tuple_for_each(const runtime_tuple & rtup, Visitor v) {
	runtime_tuple result;
	for(auto && val: rtup) {
		result.emplace_back(toRuntime(v(val)));
	}
	return toRuntime(move(result));
}

template <class Tuple>
void print(const Tuple & t, ostream & os) {
	os << "[";
	bool first = true;
	tuple_for_each(t, [&](auto && in) mutable {
		if(!first) os << ", ";
		first = false;
		print(in, os);
		return unit{};
	});
	os << "]";
}

bool phony3 = []{
		printers["tuple"] = [](const RTValue & value, ostream & os) {print(* (runtime_tuple *) value.value.get(), os);};
	return true;
}();

int parse(Proxy<int>, istream & is) {
	int result;
	is >> result;
	return result;
}

string parse(Proxy<string>, istream & is) {
	string result;
	is >> quoted(result);
	return result;
}

bool skip(char s, istream & is) {
	char c;
	while(is.get(c)) {
		if(isspace(c))   continue;
		else if (c == s) return true;
		else             return false;
	}
	return false;
}

template <class ... Elems>
tuple<Elems...> parse(Proxy<tuple<Elems...>> p, istream & is);

tuple<> parse_impl(Proxy<tuple<>>, istream & is){return {};}

template <class Elem, class ... Elems>
tuple<Elem, Elems...> parse_impl(Proxy<tuple<Elem, Elems...>>, istream & is) {
	tuple<Elem, Elems...> result {
		parse(proxy<Elem>, is),
		(skip(',', is), is.good() ? parse(proxy<Elems>, is) : Elems{})...
	};

	return result;
}

template <class ... Elems>
tuple<Elems...> parse(Proxy<tuple<Elems...>> p, istream & is) {
	tuple<Elems...> result;
	if(!skip('[', is)) {
		is.setstate(is.failbit);
		return result;
	}
	result = parse_impl(p, is);
	if(skip(']', is)) {
		is.setstate(is.failbit);
		return result;
	}
}

int main() {
	auto tup = make_tuple(1, "abc"s, make_tuple(1, "2"s , 3));
	print(tup, cout);
	cout << endl;
	print(toRuntime(tup), cout);
	cout << endl;
	istringstream sin{R"([1, "abc", 2])"};
	print(parse(proxy<tuple<int, string, int>>, sin), cout);
	cout << endl;
	cout << (RTProxy{"int"} == toRuntime(proxy<int>));
}
