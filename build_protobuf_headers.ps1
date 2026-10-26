$protoc = ".\vcpkg_installed\x86-windows-static-md\tools\protobuf\protoc.exe" 
& $protoc "statsgate.proto" "--cpp_out=.\statsrecorder"
& $protoc "statsgate.proto" "--python_out=.\scripts"
