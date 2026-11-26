#ifndef _INLINE_VECTOR_H_
#define _INLINE_VECTOR_H_ 1

template <typename Value, int capacity >
class inline_vector {

	static_assert(capacity <= 255,"inline vectors have a maximum size of 255");
	static_assert(std::is_trivially_copyable_v<Value>, "Inline vector won't provide a benefit unless the type is trivially copyable.") ;
public:
	using value_type = typename Value;

	Value data[capacity] ;
	unsigned char filled = 0 ;

	Value& operator[](std::size_t index) {
		return data[index];
	}

	void push_back(const Value& value){
		data[filled++] = value ;
	}

	void push_back(const Value&& value) {
		data[filled++] = std::move(value);
	}

	template <typename ValueArgs>
	void emplace_back(ValueArgs&& value_args){
		data[filled++] = Value(std::forward<ValueArgs>(value_args)) ;
	}

	void pop_back(){
		if(filled > 0){
			filled--;
		}
	}

	void clear(){
		filled = 0 ;
	}

	std::size_t size() const {
		return filled; 
	}

	std::size_t capacity() const {
		return capacity; 
	}

	bool empty() const {
		return filled == 0; 
	}

	//Can use raw pointers for iteratorsbecause data is contiguous
	Value* begin() {
		return &data[0];
	}

	Value* end() {
		return &data[filled];
	}

	const Value* begin() const {
		return &data[0];
	}

	const Value* end() const {
		return &data[filled];
	}

	void resize(std::size_t new_size){
		filled = new_size ;
	}

	void reserve(std::size_t new_capacity){
		//Doesn't make sense as reservation is fixed, but allows drop in replacement in code that previously used vectors
	}
};
#endif // #ifndef _INLINE_VECTOR_H_