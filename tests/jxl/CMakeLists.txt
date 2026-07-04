IF(JXL_FOUND)
ADD_DEFINITIONS(-DHAVE_LIBJXL)
LIST(APPEND TESTS_FILES
	jxl_null
	jxl_im2im
	jxl_lossless_im2im
	jxl_alpha
	jxl_read_file
	jxl_palette
	jxl_anim
	jxl_anim_lossless
	jxl_anim_raw
	jxl_still_as_anim
)

IF(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../conformance/jxl/conformance")
LIST(APPEND TESTS_FILES jxl_conformance)
MESSAGE(STATUS "JPEG XL conformance corpus found")
ELSE()
MESSAGE(STATUS "JPEG XL conformance corpus not found; skipping jxl_conformance")
ENDIF()
ENDIF(JXL_FOUND)

ADD_GD_TESTS()
