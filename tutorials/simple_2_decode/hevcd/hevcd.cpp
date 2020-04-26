#include "hevcd.h"
#include "hevc_decode.h"

typedef HEVCDecoder impl_type, *impl_ptr_type;

handle_type create_hevcd(int32_type switchtoavc)
{
	return new impl_type(switchtoavc);
}

void_type destroy_hevcd(handle_type p)
{
	delete (impl_ptr_type)p;
}

int32_type hd_decode(handle_type p, byte_ptr_type lpbframe, int32_type nframesize)
{
	return ((impl_ptr_type)p)->Decode(lpbframe, nframesize);
}

byte_ptr_type hd_getframe(handle_type p, handle_type* lpbi_out)
{
	return ((impl_ptr_type)p)->GetFrame((LPBITMAPINFO&)*lpbi_out);
}