#pragma once


#include <faiss/IndexHNSW.h>
#include <faiss/IndexIDMap.h>
#include <faiss/index_io.h>

namespace memory::f
{
	using faiss_index = faiss::IndexHNSWFlat;
}