cmake_minimum_required(VERSION 3.18)

project(HeteroSTA_Topk LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(USE_GPU "Link against GPU HeteroSTA and CUDA runtime" OFF)

set(HETEROSTA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../HeteroSTA" CACHE PATH "Path to HeteroSTA root")
set(HETEROSTA_INCLUDE_DIR "${HETEROSTA_ROOT}/include" CACHE PATH "Path to HeteroSTA headers")
set(HETEROSTA_LIB_DIR "${HETEROSTA_ROOT}/lib" CACHE PATH "Path to HeteroSTA libs")

add_library(heterosta_topk_core
	src/topk_api.cpp
)

add_library(heterosta_topk_router
	src/simple_3bend_router.cpp
)

target_include_directories(heterosta_topk_core
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/include
		${HETEROSTA_INCLUDE_DIR}
)

target_include_directories(heterosta_topk_router
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(heterosta_topk
	src/main.cpp
)

add_executable(simple_3bend_router_demo
	src/simple_router_main.cpp
)

target_link_libraries(heterosta_topk
	PRIVATE
		heterosta_topk_core
		heterosta_topk_router
)

target_link_libraries(simple_3bend_router_demo
	PRIVATE
		heterosta_topk_router
)

find_package(Threads REQUIRED)
find_library(DL_LIB dl REQUIRED)
find_library(RT_LIB rt REQUIRED)
find_library(GOMP_LIB gomp REQUIRED)

if(USE_GPU)
	find_library(HETEROSTA_CORE_LIB NAMES heterosta PATHS ${HETEROSTA_LIB_DIR} NO_DEFAULT_PATH REQUIRED)
	find_package(CUDAToolkit REQUIRED)
	find_library(CUDA_DRIVER_LIB cuda)

	target_link_libraries(heterosta_topk_core
		PRIVATE
			${HETEROSTA_CORE_LIB}
			CUDA::cudart
			Threads::Threads
			${DL_LIB}
			${RT_LIB}
			${GOMP_LIB}
	)

	if(CUDA_DRIVER_LIB)
		target_link_libraries(heterosta_topk_core PRIVATE ${CUDA_DRIVER_LIB})
	endif()
else()
	find_library(HETEROSTA_CORE_LIB NAMES heterosta.cpu PATHS ${HETEROSTA_LIB_DIR} NO_DEFAULT_PATH REQUIRED)

	target_link_libraries(heterosta_topk_core
		PRIVATE
			${HETEROSTA_CORE_LIB}
			Threads::Threads
			${DL_LIB}
			${RT_LIB}
			${GOMP_LIB}
	)
endif()

set_target_properties(heterosta_topk_core PROPERTIES
	BUILD_RPATH "${HETEROSTA_LIB_DIR}"
	INSTALL_RPATH "${HETEROSTA_LIB_DIR}"
)

set_target_properties(heterosta_topk PROPERTIES
	BUILD_RPATH "${HETEROSTA_LIB_DIR}"
	INSTALL_RPATH "${HETEROSTA_LIB_DIR}"
)

message(STATUS "USE_GPU=${USE_GPU}")
message(STATUS "HETEROSTA_ROOT=${HETEROSTA_ROOT}")
message(STATUS "HETEROSTA_INCLUDE_DIR=${HETEROSTA_INCLUDE_DIR}")
message(STATUS "HETEROSTA_LIB_DIR=${HETEROSTA_LIB_DIR}")
