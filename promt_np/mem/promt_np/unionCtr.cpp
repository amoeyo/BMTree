#include "unionCtr.h"

status reg_data_move(void* dst, void* src, size_t n_bytes) {
	//参数检查
	//寄存器数据搬动，暂时不引入时延
	memcpy(dst, src, n_bytes);
	return SUCCESS;
}

status 
RegisterUnionCtr::getValue(uint32_t ctr_K, major_t* major, uint64_t* minor) {
	//check ctr_K < 64
	__get_major(major);
	__get_K_minor(ctr_K, minor); //从低字节开始填充
	return SUCCESS;
}

void RegisterUnionCtr::__set_major(major_t major) { __major = major; }
void RegisterUnionCtr::__get_major(major_t* major) { *major = __major; }

status RegisterUnionCtr::__set_K_minor(table_size_t idx_K, uint64_t value) {
	return ((__RegisterTool*)this)->set_value(idx_K * 7, 0x7f, value);
}

status RegisterUnionCtr::__get_K_minor(table_size_t idx_K, uint64_t* value) {
	return ((__RegisterTool*)this)->get_value(idx_K * 7, 0x7f, value);
}

status RegisterUnionCtr::set_K_minor(table_size_t idx_K, uint64_t minor) {
	if (minor == 0x80) {
		__set_K_minor(idx_K, 0);
#ifdef IGNORE_MINOR_OVERFLOW
		return SUCCESS;
#else
		__set_major(__major + 1); //直接操作大寄存器，不会带来时延
		return MINOR_OVERFLOW;//同个UnionCtr的其他OTP需要重新生成，数据重新加密
#endif // IGNORE_MINOR_OVERFLOW
	}
	else {
		__set_K_minor(idx_K, minor);
	}
	return SUCCESS;
}

