#!/bin/bash

# Live Blueprint Collaboration Plugin - Build and Test Script
# This script helps build and test the plugin in a UE5.5 project

set -e  # Exit on any error

# Configuration
UE5_PATH="/Users/Shared/Epic Games/UE_5.5/Engine"
PROJECT_NAME="LiveBPTest"
PLUGIN_NAME="LiveBlueprint"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}Live Blueprint Collaboration Plugin - Build Script${NC}"
echo "=================================================="

# Function to print status messages
print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if UE5 is installed
if [ ! -d "$UE5_PATH" ]; then
    print_error "Unreal Engine 5.5 not found at: $UE5_PATH"
    print_warning "Please update UE5_PATH in this script to point to your UE5.5 installation"
    exit 1
fi

print_status "Found Unreal Engine 5.5 at: $UE5_PATH"

# Check if we're in the plugin directory
if [ ! -f "LiveBlueprint.uplugin" ]; then
    print_error "LiveBlueprint.uplugin not found. Please run this script from the plugin root directory."
    exit 1
fi

print_status "Found plugin descriptor: LiveBlueprint.uplugin"

# Create test project if it doesn't exist
if [ ! -d "../$PROJECT_NAME" ]; then
    print_status "Creating test project: $PROJECT_NAME"
    
    # Create basic project structure
    mkdir -p "../$PROJECT_NAME"
    mkdir -p "../$PROJECT_NAME/Content"
    mkdir -p "../$PROJECT_NAME/Config"
    mkdir -p "../$PROJECT_NAME/Plugins"
    
    # Create basic .uproject file
    cat > "../$PROJECT_NAME/$PROJECT_NAME.uproject" << EOF
{
    "FileVersion": 3,
    "EngineAssociation": "5.5",
    "Category": "",
    "Description": "Test project for Live Blueprint Collaboration Plugin",
    "Modules": [
        {
            "Name": "$PROJECT_NAME",
            "Type": "Runtime",
            "LoadingPhase": "Default"
        }
    ],
    "Plugins": [
        {
            "Name": "Concert",
            "Enabled": true
        },
        {
            "Name": "MultiUserClient", 
            "Enabled": true
        },
        {
            "Name": "$PLUGIN_NAME",
            "Enabled": true
        }
    ]
}
EOF

    # Create basic module files
    mkdir -p "../$PROJECT_NAME/Source/$PROJECT_NAME"
    
    cat > "../$PROJECT_NAME/Source/$PROJECT_NAME/$PROJECT_NAME.Build.cs" << EOF
using UnrealBuildTool;

public class $PROJECT_NAME : ModuleRules
{
    public $PROJECT_NAME(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject", 
            "Engine"
        });
    }
}
EOF

    cat > "../$PROJECT_NAME/Source/$PROJECT_NAME/$PROJECT_NAME.cpp" << EOF
#include "$PROJECT_NAME.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, $PROJECT_NAME, "$PROJECT_NAME");
EOF

    cat > "../$PROJECT_NAME/Source/$PROJECT_NAME/$PROJECT_NAME.h" << EOF
#pragma once

#include "CoreMinimal.h"
EOF

    # Create target files
    cat > "../$PROJECT_NAME/Source/$PROJECT_NAME.Target.cs" << EOF
using UnrealBuildTool;

public class ${PROJECT_NAME}Target : TargetRules
{
    public ${PROJECT_NAME}Target(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V4;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("$PROJECT_NAME");
    }
}
EOF

    cat > "../$PROJECT_NAME/Source/${PROJECT_NAME}Editor.Target.cs" << EOF
using UnrealBuildTool;

public class ${PROJECT_NAME}EditorTarget : TargetRules
{
    public ${PROJECT_NAME}EditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V4;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("$PROJECT_NAME");
    }
}
EOF

    print_status "Created test project: $PROJECT_NAME"
else
    print_status "Using existing test project: $PROJECT_NAME"
fi

# Link or copy plugin to test project
PLUGIN_DIR="../$PROJECT_NAME/Plugins/$PLUGIN_NAME"
if [ ! -L "$PLUGIN_DIR" ] && [ ! -d "$PLUGIN_DIR" ]; then
    print_status "Linking plugin to test project"
    ln -s "$(pwd)" "$PLUGIN_DIR"
elif [ -L "$PLUGIN_DIR" ]; then
    print_status "Plugin already linked to test project"
else
    print_warning "Plugin directory exists but is not a symlink. Please check manually."
fi

# Generate project files
print_status "Generating project files..."
cd "../$PROJECT_NAME"

if [[ "$OSTYPE" == "darwin"* ]]; then
    # macOS
    "$UE5_PATH/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe" -projectfiles -project="$(pwd)/$PROJECT_NAME.uproject" -game -rocket -progress
else
    # Windows (if running under WSL or similar)
    "$UE5_PATH/Binaries/DotNET/UnrealBuildTool.exe" -projectfiles -project="$(pwd)/$PROJECT_NAME.uproject" -game -rocket -progress
fi

print_status "Project files generated successfully"

# Build the project
print_status "Building project and plugin..."

if [[ "$OSTYPE" == "darwin"* ]]; then
    # macOS - build with Xcode
    "$UE5_PATH/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe" "${PROJECT_NAME}Editor" Mac Development -Project="$(pwd)/$PROJECT_NAME.uproject" -WaitMutex -FromMsBuild
else
    # Windows
    "$UE5_PATH/Binaries/DotNET/UnrealBuildTool.exe" "${PROJECT_NAME}Editor" Win64 Development -Project="$(pwd)/$PROJECT_NAME.uproject" -WaitMutex -FromMsBuild
fi

if [ $? -eq 0 ]; then
    print_status "Build completed successfully!"
else
    print_error "Build failed. Please check the output above for errors."
    exit 1
fi

# Run basic tests
print_status "Running plugin tests..."
cd "$PLUGIN_DIR"

# Check if all expected files exist
EXPECTED_FILES=(
    "LiveBlueprint.uplugin"
    "Source/LiveBPCore/LiveBPCore.Build.cs"
    "Source/LiveBPEditor/LiveBPEditor.Build.cs"
    "Source/LiveBPCore/Public/LiveBPCore.h"
    "Source/LiveBPEditor/Public/LiveBPEditor.h"
    "README.md"
)

print_status "Checking plugin file integrity..."
for file in "${EXPECTED_FILES[@]}"; do
    if [ -f "$file" ]; then
        echo -e "  ✓ $file"
    else
        print_error "Missing file: $file"
        exit 1
    fi
done

print_status "All plugin files verified!"

# Optional: Launch the editor
read -p "Would you like to launch the Unreal Editor with the test project? (y/n): " -n 1 -r
echo
if [[ $REPO =~ ^[Yy]$ ]]; then
    print_status "Launching Unreal Editor..."
    cd "../$PROJECT_NAME"
    "$UE5_PATH/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$(pwd)/$PROJECT_NAME.uproject" &
    print_status "Editor launched. Check the output log for any plugin loading issues."
fi

echo
echo -e "${GREEN}Build and test completed successfully!${NC}"
echo "=================================================="
echo "Test project location: $(pwd)/../$PROJECT_NAME"
echo "Plugin location: $PLUGIN_DIR"
echo ""
echo "Next steps:"
echo "1. Open the test project in Unreal Editor"
echo "2. Enable Multi-User Editing in Project Settings"
echo "3. Create a Blueprint to test collaboration features"
echo "4. Use console commands like 'LiveBP.Help' to explore the plugin"
echo ""
echo "For more information, see README.md and DEVELOPMENT.md"
