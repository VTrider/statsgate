$protoc = ".\vcpkg_installed\x86-windows-static-md\tools\protobuf\protoc.exe" 
& $protoc "statsgate.proto" "--cpp_out=.\src"
& $protoc "statsgate.proto" "--python_out=.\scripts"
