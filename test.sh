#!/bin/bash
mkdir -p sample_project
cd sample_project
echo "Creating a new C project with CPM..."
cpm init my_app
echo "Sample main.c file..."
cat > main.c << 'EOF'
#include <stdio.h>
#include <libfoo.c>  // From a cpm package
int main() {
    printf("Hello from CPM-managed project!\n");
    printf("%d\n", add(1, 1));
    return 0;
}
EOF
echo "Adding libfoo package to cpmfile..."
cpm add libfoo 1.0
echo "Installing packages..."
cpm install
echo "Building the project..."
make
echo ===OUTPUT===
./my_app
echo ===OUTPUT===
echo "Done!"
