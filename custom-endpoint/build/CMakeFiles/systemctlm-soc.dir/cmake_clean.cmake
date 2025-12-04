file(REMOVE_RECURSE
  "libsystemctlm-soc.a"
  "libsystemctlm-soc.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang C CXX)
  include(CMakeFiles/systemctlm-soc.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
