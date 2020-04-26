#pragma once

extern "C"
{
	typedef void* handle_type;
	typedef void void_type;
	typedef int int32_type;
	typedef unsigned char* byte_ptr_type;
	
	handle_type create_hevcd(int32_type switchtoavc);
	void_type destroy_hevcd(handle_type p);

	int32_type hd_decode(handle_type p, byte_ptr_type lpbframe, int32_type nframesize);
	byte_ptr_type hd_getframe(handle_type p, handle_type* lpbi_out);
}