file(REMOVE_RECURSE
  "libwasmedge.pdb"
  "libwasmedge.so"
  "libwasmedge.so.0"
  "libwasmedge.so.0.0.3"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/wasmedge_shared.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
