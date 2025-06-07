#!/bin/bash

# LiveBlueprint Plugin Build and Test Script for UE 5.5
# This script helps build and test the plugin on macOS and Windows

set -e  # Exit on any error

# Configuration
UE_ENGINE_PATH=${UE_ENGINE_PATH:-"/Volumes/T7/unreal engine/engine/UE_5.5"}
PLUGIN_NAME="LiveBlueprint"
PLUGIN_PATH="$(pwd)"
BUILD_CONFIGURATION="Development"
TARGET_PLATFORMS="Mac"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

echo_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

echo_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to clean plugin build artifacts
clean_plugin() {
    echo_info "Cleaning plugin build artifacts..."
    
    # Remove Binaries and Intermediate folders from plugin root
    if [ -d "Binaries" ]; then
        echo_info "Removing plugin Binaries folder..."
        rm -rf "Binaries"
    fi
    
    if [ -d "Intermediate" ]; then
        echo_info "Removing plugin Intermediate folder..."
        rm -rf "Intermediate"
    fi
    
    # Remove any leftover empty module directories (fixed in script)
    if [ -d "Source/LiveBlueprintCoop" ]; then
        echo_warning "Removing empty LiveBlueprintCoop module directory..."
        rm -rf "Source/LiveBlueprintCoop"
    fi
    
    if [ -d "Source/LiveBlueprintCoopEditor" ]; then
        echo_warning "Removing empty LiveBlueprintCoopEditor module directory..."
        rm -rf "Source/LiveBlueprintCoopEditor"
    fi
    
    echo_info "Plugin cleanup complete."
}

# Function to validate plugin structure
validate_plugin() {
    echo_info "Validating plugin structure..."
    
    # Check .uplugin file exists
    if [ ! -f "${PLUGIN_NAME}.uplugin" ]; then
        echo_error "Plugin descriptor file ${PLUGIN_NAME}.uplugin not found!"
        exit 1
    fi
    
    # Check required modules exist
    for module in "LiveBPCore" "LiveBPEditor"; do
        if [ ! -d "Source/${module}" ]; then
            echo_error "Required module ${module} not found!"
            exit 1
        fi
        
        if [ ! -f "Source/${module}/${module}.Build.cs" ]; then
            echo_error "Build file for module ${module} not found!"
            exit 1
        fi
    done
    
    echo_info "Plugin structure validation passed."
}

# Function to check UE 5.5 requirements
check_ue_requirements() {
    echo_info "Checking UE 5.5 requirements..."
    
    # Check if UE path exists
    if [ ! -d "${UE_ENGINE_PATH}" ]; then
        echo_error "Unreal Engine path not found: ${UE_ENGINE_PATH}"
        echo_info "Please set UE_ENGINE_PATH environment variable or update the script"
        exit 1
    fi
    
    # Check for RunUAT script
    if [ ! -f "${UE_ENGINE_PATH}/Engine/Build/BatchFiles/RunUAT.sh" ]; then
        echo_error "RunUAT script not found in engine directory"
        exit 1
    fi
    
    echo_info "UE 5.5 requirements check passed."
}

# Function to build plugin using RunUAT
build_plugin() {
    echo_info "Building plugin with RunUAT..."
    
    local output_dir="${PLUGIN_PATH}/Built"
    
    # Create output directory
    mkdir -p "${output_dir}"
    
    # Build command for UE 5.5
    local build_cmd="${UE_ENGINE_PATH}/Engine/Build/BatchFiles/RunUAT.sh"
    build_cmd+=" BuildPlugin"
    build_cmd+=" -Plugin=\"${PLUGIN_PATH}/${PLUGIN_NAME}.uplugin\""
    build_cmd+=" -Package=\"${output_dir}\""
    build_cmd+=" -TargetPlatforms=${TARGET_PLATFORMS}"
    build_cmd+=" -Rocket"
    
    echo_info "Running build command:"
    echo "${build_cmd}"
    
    # Execute build
    if eval "${build_cmd}"; then
        echo_info "Plugin build completed successfully!"
        return 0
    else
        echo_error "Plugin build failed!"
        return 1
    fi
}

# Function to create a test project for plugin validation
create_test_project() {
    echo_info "Creating test project for plugin validation..."
    
    local test_project_dir="${PLUGIN_PATH}/TestProject"
    local test_project_name="LiveBPTest"
    
    # Remove existing test project
    if [ -d "${test_project_dir}" ]; then
        rm -rf "${test_project_dir}"
    fi
    
    mkdir -p "${test_project_dir}"
    
    # Create minimal .uproject file
    cat > "${test_project_dir}/${test_project_name}.uproject" << EOF
{
    "FileVersion": 3,
    "EngineAssociation": "5.5",
    "Category": "",
    "Description": "Test project for LiveBlueprint plugin",
    "Modules": [
        {
            "Name": "${test_project_name}",
            "Type": "Runtime",
            "LoadingPhase": "Default"
        }
    ],
    "Plugins": [
        {
            "Name": "LiveBlueprint",
            "Enabled": true
        }
    ]
}
EOF
    
    # Create plugins directory and copy our plugin
    mkdir -p "${test_project_dir}/Plugins"
    cp -r "${PLUGIN_PATH}" "${test_project_dir}/Plugins/${PLUGIN_NAME}"
    
    # Remove build artifacts from copied plugin
    rm -rf "${test_project_dir}/Plugins/${PLUGIN_NAME}/Binaries"
    rm -rf "${test_project_dir}/Plugins/${PLUGIN_NAME}/Intermediate"
    rm -rf "${test_project_dir}/Plugins/${PLUGIN_NAME}/Built"
    rm -rf "${test_project_dir}/Plugins/${PLUGIN_NAME}/TestProject"
    
    echo_info "Test project created at: ${test_project_dir}"
}

# Function to test plugin compilation in project context
test_plugin_in_project() {
    echo_info "Testing plugin compilation in project context..."
    
    local test_project_dir="${PLUGIN_PATH}/TestProject"
    local test_project_name="LiveBPTest"
    
    if [ ! -d "${test_project_dir}" ]; then
        echo_error "Test project not found. Run create_test_project first."
        return 1
    fi
    
    # Try to compile the project with our plugin
    local compile_cmd="${UE_ENGINE_PATH}/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool"
    compile_cmd+=" ${test_project_name}Editor"
    compile_cmd+=" Mac"
    compile_cmd+=" Development"
    compile_cmd+=" -Project=\"${test_project_dir}/${test_project_name}.uproject\""
    compile_cmd+=" -Rocket"
    
    echo_info "Running project compilation test:"
    echo "${compile_cmd}"
    
    # Change to test project directory
    pushd "${test_project_dir}" > /dev/null
    
    if eval "${compile_cmd}"; then
        echo_info "Plugin compilation test in project context passed!"
        popd > /dev/null
        return 0
    else
        echo_error "Plugin compilation test in project context failed!"
        popd > /dev/null
        return 1
    fi
}

# Main execution
main() {
    echo_info "Starting LiveBlueprint Plugin Build and Test for UE 5.5"
    echo_info "Plugin path: ${PLUGIN_PATH}"
    echo_info "Engine path: ${UE_ENGINE_PATH}"
    
    # Parse command line arguments
    case "${1:-build}" in
        "clean")
            clean_plugin
            ;;
        "validate")
            validate_plugin
            ;;
        "build")
            check_ue_requirements
            validate_plugin
            clean_plugin
            build_plugin
            ;;
        "test")
            check_ue_requirements
            validate_plugin
            clean_plugin
            create_test_project
            test_plugin_in_project
            ;;
        "full")
            check_ue_requirements
            validate_plugin
            clean_plugin
            build_plugin
            create_test_project
            test_plugin_in_project
            ;;
        *)
            echo_info "Usage: $0 [clean|validate|build|test|full]"
            echo_info "  clean    - Clean build artifacts"
            echo_info "  validate - Validate plugin structure"
            echo_info "  build    - Build plugin using RunUAT"
            echo_info "  test     - Test plugin in project context"
            echo_info "  full     - Run complete build and test cycle"
            exit 1
            ;;
    esac
    
    echo_info "Operation completed successfully!"
}

# Run main function with all arguments
main "$@"
