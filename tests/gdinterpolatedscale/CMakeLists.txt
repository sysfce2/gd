LIST(APPEND TESTS_FILES
	alpha_weight
	gdBicubicFixed
	gdModesAndPalettes
	gdTrivialResize
	linear_light
	scale_flow
)

ADD_GD_TESTS()

# Probably should speed this test up.
set_tests_properties(test_${TEST_PREFIX}_gdTrivialResize PROPERTIES TIMEOUT 20)
