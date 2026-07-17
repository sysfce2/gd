LIST(APPEND TESTS_FILES
	alpha_weight
	edge
	fractional
	interpolation
	matrix
	rotate
)

IF(PNG_FOUND)
	LIST(APPEND TESTS_FILES visual)
ENDIF(PNG_FOUND)

ADD_GD_TESTS()
