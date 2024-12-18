if (-not (Test-Path -Path "build")) {
    New-Item -ItemType Directory -Path "build"
}

Write-Host "Compiling client-hello-windows.cpp"
g++ -O3 -std=c++17 -I. client-hello-windows.cpp -o build/client-hello-windows -lwsock32 -lws2_32

Write-Host "Compiling server-hello-windows.cpp"
g++ -O3 -std=c++17 -I. server-hello-windows.cpp -o build/server-hello-windows -lwsock32 -lws2_32

Write-Host "Compile complete!"