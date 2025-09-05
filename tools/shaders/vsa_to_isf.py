#!/usr/bin/env python3
"""
VSA to ISF Converter
Converts Vertex Shader Art JSON files to ISF-like format for ossia score
"""

import json
import sys
import os
import re
import argparse
from typing import Dict, Any, List

def convert_mode(mode: str) -> str:
    """Convert VSA mode to ISF primitive mode"""
    mode_map = {
        "POINTS": "POINTS",
        "LINES": "LINES", 
        "LINE_STRIP": "LINE_STRIP",
        "LINE_LOOP": "LINE_LOOP",
        "TRIANGLES": "TRIANGLES",
        "TRIANGLE_STRIP": "TRIANGLE_STRIP",
        "TRIANGLE_FAN": "TRIANGLE_FAN"
    }
    return mode_map.get(mode, "POINTS")

def sanitize_filename(name: str) -> str:
    """Sanitize a string for use as a filename"""
    # Replace or remove invalid filename characters
    sanitized = re.sub(r'[<>:"/\\|?*]', '_', name)  # Windows invalid chars
    sanitized = re.sub(r'[^\w\s\-_.]', '_', sanitized)  # Keep only word chars, spaces, hyphens, underscores, dots
    sanitized = re.sub(r'\s+', '_', sanitized)  # Replace spaces with underscores
    sanitized = re.sub(r'_{2,}', '_', sanitized)  # Replace multiple underscores with single
    sanitized = sanitized.strip('_.')  # Remove leading/trailing underscores and dots
    
    # Ensure it's not empty and not too long
    if not sanitized:
        sanitized = "untitled"
    elif len(sanitized) > 100:
        sanitized = sanitized[:100]
    
    return sanitized

def get_unique_filename(base_name: str, extension: str = ".vs") -> str:
    """Generate a unique filename by appending a counter if the file exists"""
    filename = f"{base_name}{extension}"
    
    if not os.path.exists(filename):
        return filename
    
    counter = 1
    while True:
        filename = f"{base_name}-{counter}{extension}"
        if not os.path.exists(filename):
            return filename
        counter += 1

def prune_builtin_functions(shader_code: str) -> str:
    """Remove GLSL built-in functions that are redundantly defined"""
    
    # Define patterns for built-in GLSL functions that shouldn't be redefined
    builtin_patterns = [
        # Matrix functions
        r'mat4\s+transpose\s*\(\s*mat4\s+\w+\s*\)\s*\{[^}]*\}',
        r'mat3\s+transpose\s*\(\s*mat3\s+\w+\s*\)\s*\{[^}]*\}',
        r'mat2\s+transpose\s*\(\s*mat2\s+\w+\s*\)\s*\{[^}]*\}',
        r'float\s+determinant\s*\(\s*mat\d+\s+\w+\s*\)\s*\{[^}]*\}',
        r'mat\d+\s+inverse\s*\(\s*mat\d+\s+\w+\s*\)\s*\{[^}]*\}',
        
        # Vector functions
        r'float\s+length\s*\(\s*vec\d+\s+\w+\s*\)\s*\{[^}]*\}',
        r'float\s+distance\s*\(\s*vec\d+\s+\w+\s*,\s*vec\d+\s+\w+\s*\)\s*\{[^}]*\}',
        r'float\s+dot\s*\(\s*vec\d+\s+\w+\s*,\s*vec\d+\s+\w+\s*\)\s*\{[^}]*\}',
        r'vec3\s+cross\s*\(\s*vec3\s+\w+\s*,\s*vec3\s+\w+\s*\)\s*\{[^}]*\}',
        r'vec\d+\s+normalize\s*\(\s*vec\d+\s+\w+\s*\)\s*\{[^}]*\}',
        r'vec\d+\s+reflect\s*\(\s*vec\d+\s+\w+\s*,\s*vec\d+\s+\w+\s*\)\s*\{[^}]*\}',
        r'vec\d+\s+refract\s*\(\s*vec\d+\s+\w+\s*,\s*vec\d+\s+\w+\s*,\s*float\s+\w+\s*\)\s*\{[^}]*\}',
        
        # Math functions
        r'float\s+radians\s*\(\s*float\s+\w+\s*\)\s*\{[^}]*\}',
        r'float\s+degrees\s*\(\s*float\s+\w+\s*\)\s*\{[^}]*\}',
        r'float\s+sin\s*\(\s*float\s+\w+\s*\)\s*\{[^}]*\}',
        r'float\s+cos\s*\(\s*float\s+\w+\s*\)\s*\{[^}]*\}',
        r'float\s+tan\s*\(\s*float\s+\w+\s*\)\s*\{[^}]*\}',
        r'float\s+asin\s*\(\s*float\s+\w+\s*\)\s*\{[^}]*\}',
        r'float\s+acos\s*\(\s*float\s+\w+\s*\)\s*\{[^}]*\}',
        r'float\s+atan\s*\(\s*float\s+\w+\s*\)\s*\{[^}]*\}',
        r'float\s+pow\s*\(\s*float\s+\w+\s*,\s*float\s+\w+\s*\)\s*\{[^}]*\}',
        r'float\s+exp\s*\(\s*float\s+\w+\s*\)\s*\{[^}]*\}',
        r'float\s+log\s*\(\s*float\s+\w+\s*\)\s*\{[^}]*\}',
        r'float\s+sqrt\s*\(\s*float\s+\w+\s*\)\s*\{[^}]*\}',
        r'float\s+abs\s*\(\s*float\s+\w+\s*\)\s*\{[^}]*\}',
        r'float\s+sign\s*\(\s*float\s+\w+\s*\)\s*\{[^}]*\}',
        r'float\s+floor\s*\(\s*float\s+\w+\s*\)\s*\{[^}]*\}',
        r'float\s+ceil\s*\(\s*float\s+\w+\s*\)\s*\{[^}]*\}',
        r'float\s+fract\s*\(\s*float\s+\w+\s*\)\s*\{[^}]*\}',
        r'float\s+mod\s*\(\s*float\s+\w+\s*,\s*float\s+\w+\s*\)\s*\{[^}]*\}',
        r'float\s+min\s*\(\s*float\s+\w+\s*,\s*float\s+\w+\s*\)\s*\{[^}]*\}',
        r'float\s+max\s*\(\s*float\s+\w+\s*,\s*float\s+\w+\s*\)\s*\{[^}]*\}',
        r'float\s+clamp\s*\(\s*float\s+\w+\s*,\s*float\s+\w+\s*,\s*float\s+\w+\s*\)\s*\{[^}]*\}',
        r'float\s+mix\s*\(\s*float\s+\w+\s*,\s*float\s+\w+\s*,\s*float\s+\w+\s*\)\s*\{[^}]*\}',
        r'float\s+step\s*\(\s*float\s+\w+\s*,\s*float\s+\w+\s*\)\s*\{[^}]*\}',
        r'float\s+smoothstep\s*\(\s*float\s+\w+\s*,\s*float\s+\w+\s*,\s*float\s+\w+\s*\)\s*\{[^}]*\}',
        
        # Vector versions of math functions
        r'vec\d+\s+sin\s*\(\s*vec\d+\s+\w+\s*\)\s*\{[^}]*\}',
        r'vec\d+\s+cos\s*\(\s*vec\d+\s+\w+\s*\)\s*\{[^}]*\}',
        r'vec\d+\s+tan\s*\(\s*vec\d+\s+\w+\s*\)\s*\{[^}]*\}',
        r'vec\d+\s+abs\s*\(\s*vec\d+\s+\w+\s*\)\s*\{[^}]*\}',
        r'vec\d+\s+floor\s*\(\s*vec\d+\s+\w+\s*\)\s*\{[^}]*\}',
        r'vec\d+\s+ceil\s*\(\s*vec\d+\s+\w+\s*\)\s*\{[^}]*\}',
        r'vec\d+\s+fract\s*\(\s*vec\d+\s+\w+\s*\)\s*\{[^}]*\}',
        r'vec\d+\s+mod\s*\(\s*vec\d+\s+\w+\s*,\s*(?:vec\d+|float)\s+\w+\s*\)\s*\{[^}]*\}',
        r'vec\d+\s+min\s*\(\s*vec\d+\s+\w+\s*,\s*(?:vec\d+|float)\s+\w+\s*\)\s*\{[^}]*\}',
        r'vec\d+\s+max\s*\(\s*vec\d+\s+\w+\s*,\s*(?:vec\d+|float)\s+\w+\s*\)\s*\{[^}]*\}',
        r'vec\d+\s+clamp\s*\(\s*vec\d+\s+\w+\s*,\s*(?:vec\d+|float)\s+\w+\s*,\s*(?:vec\d+|float)\s+\w+\s*\)\s*\{[^}]*\}',
        r'vec\d+\s+mix\s*\(\s*vec\d+\s+\w+\s*,\s*vec\d+\s+\w+\s*,\s*(?:vec\d+|float)\s+\w+\s*\)\s*\{[^}]*\}',
        r'vec\d+\s+step\s*\(\s*(?:vec\d+|float)\s+\w+\s*,\s*vec\d+\s+\w+\s*\)\s*\{[^}]*\}',
        r'vec\d+\s+smoothstep\s*\(\s*(?:vec\d+|float)\s+\w+\s*,\s*(?:vec\d+|float)\s+\w+\s*,\s*vec\d+\s+\w+\s*\)\s*\{[^}]*\}',
        
        # Texture functions (older GLSL versions)
        r'vec4\s+texture2D\s*\(\s*sampler2D\s+\w+\s*,\s*vec2\s+\w+\s*\)\s*\{[^}]*\}',
        r'vec4\s+textureCube\s*\(\s*samplerCube\s+\w+\s*,\s*vec3\s+\w+\s*\)\s*\{[^}]*\}',
    ]
    
    # Remove redundant function definitions
    result = shader_code
    removed_functions = []
    
    for pattern in builtin_patterns:
        matches = list(re.finditer(pattern, result, re.MULTILINE | re.DOTALL))
        for match in matches:
            function_def = match.group(0)
            # Extract function name for logging
            func_name_match = re.search(r'(\w+)\s+(\w+)\s*\(', function_def)
            if func_name_match:
                func_name = func_name_match.group(2)
                removed_functions.append(func_name)
            
            result = result.replace(function_def, '')
    
    # Clean up multiple empty lines that might result from removals
    result = re.sub(r'\n\s*\n\s*\n+', '\n\n', result)
    
    return result, removed_functions

def clean_shader_code(shader_code: str, prune_builtins: bool = True) -> str:
    """Clean and optimize shader code"""
    removed_funcs = []
    
    # First prune built-in functions if enabled
    if prune_builtins:
        pruned_code, removed_funcs = prune_builtin_functions(shader_code)
    else:
        pruned_code = shader_code
    
    # Remove excessive whitespace but preserve intentional formatting
    lines = pruned_code.split('\n')
    cleaned_lines = []
    
    for line in lines:
        # Preserve indentation but clean up excessive spaces
        stripped = line.strip()
        if stripped:
            # Count leading spaces for indentation
            indent = len(line) - len(line.lstrip())
            # Normalize excessive spaces in code
            normalized = re.sub(r'\s+', ' ', stripped)
            cleaned_lines.append(' ' * min(indent, 8) + normalized)
        else:
            # Preserve empty lines for readability
            cleaned_lines.append('')
    
    result = '\n'.join(cleaned_lines)
    
    # Return both the cleaned code and info about removed functions for logging
    return result if not removed_funcs else result + f"\n// Removed built-in GLSL functions: {', '.join(removed_funcs)}"

def infer_categories(shader_code: str, name: str, settings: Dict[str, Any]) -> List[str]:
    """Infer categories based on shader content and metadata"""
    categories = []
    
    # Analyze shader code patterns
    code_lower = shader_code.lower()
    name_lower = name.lower()
    
    # Geometric patterns
    if any(pattern in code_lower for pattern in ['sphere', 'cube', 'torus', 'plane']):
        categories.append("Geometry")
    
    # Mathematical patterns
    if any(pattern in code_lower for pattern in ['sin(', 'cos(', 'tan(', 'noise', 'random']):
        categories.append("Math")
    
    # Animation patterns  
    if 'time' in code_lower:
        categories.append("Animated")
    
    # Particle systems
    if settings.get("mode") == "POINTS" and settings.get("num", 0) > 100:
        categories.append("Particles")
    
    # Visual effects
    if any(pattern in code_lower for pattern in ['glow', 'blur', 'distort', 'wave']):
        categories.append("Effects")
    
    # Nature themes
    if any(pattern in name_lower for pattern in ['water', 'fire', 'cloud', 'star', 'galaxy', 'ocean']):
        categories.append("Nature")
    
    # Abstract themes
    if any(pattern in name_lower for pattern in ['abstract', 'fractal', 'pattern', 'art']):
        categories.append("Abstract")
    
    return categories

def convert_vsa_to_isf(vsa_data: Dict[str, Any], prune_builtins: bool = True) -> str:
    """Convert VSA JSON data to ISF-like vertex shader format"""
    
    settings = vsa_data.get("settings", {})
    
    # Extract metadata
    name = vsa_data.get("name", "Untitled")
    owner = vsa_data.get("owner", {}).get("username", "unknown")
    vsa_id = vsa_data.get("_id", "")
    notes = vsa_data.get("notes", "")
    
    # Create description and credit
    description = name
    if notes:
        description += f" - {notes}"
    credit = f"{owner} (ported from https://www.vertexshaderart.com/art/{vsa_id})"
    
    # Extract settings with validation
    point_count = max(1, min(100000, settings.get("num", 1000)))  # Clamp to reasonable range
    mode = convert_mode(settings.get("mode", "POINTS"))
    line_size = settings.get("lineSize", "NATIVE")
    background_color = settings.get("backgroundColor", [0, 0, 0, 1])
    shader_code = settings.get("shader", "")
    
    # Validate background color
    if not isinstance(background_color, list) or len(background_color) != 4:
        background_color = [0, 0, 0, 1]
    
    # Clean shader code
    cleaned_shader = clean_shader_code(shader_code, prune_builtins)
    
    # Infer categories
    categories = infer_categories(cleaned_shader, name, settings)
    
    # Add metadata from VSA
    metadata = {}
    if vsa_data.get("views"):
        metadata["ORIGINAL_VIEWS"] = vsa_data["views"]
    if vsa_data.get("likes"):
        metadata["ORIGINAL_LIKES"] = vsa_data["likes"]
    if vsa_data.get("createdAt"):
        metadata["ORIGINAL_DATE"] = vsa_data["createdAt"]
    
    # Create ISF header
    isf_header = {
        "DESCRIPTION": description,
        "CREDIT": credit,
        "ISFVSN": "2",
        "MODE": "VERTEX_SHADER_ART",
        "CATEGORIES": categories,
        "POINT_COUNT": point_count,
        "PRIMITIVE_MODE": mode,
        "LINE_SIZE": line_size,
        "BACKGROUND_COLOR": background_color,
        "INPUTS": [ ]
    }
    
    # Add metadata if present
    if metadata:
        isf_header["METADATA"] = metadata
    
    # Format the header as a JSON comment
    header_json = json.dumps(isf_header, indent=2)
    header_comment = "/*" + header_json + "*/\n\n"
    
    # Combine header and shader
    result = header_comment + cleaned_shader
    
    return result

def main():
    parser = argparse.ArgumentParser(
        description="Convert Vertex Shader Art JSON files to ISF-like format for ossia score",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python3 vsa_to_isf.py shader.json
  python3 vsa_to_isf.py shader.json -o output.vsa
  python3 vsa_to_isf.py shader.json --no-inputs --categories "Art,Animation"
  python3 vsa_to_isf.py shader.json --validate --verbose
        """
    )
    
    parser.add_argument('input', help='Input VSA JSON file')
    parser.add_argument('-o', '--output', help='Output file path (default: vsa_name.vs)')
    parser.add_argument('--no-prune', action='store_true', 
                       help='Disable pruning of built-in GLSL functions')
    parser.add_argument('--validate', action='store_true', 
                       help='Validate shader syntax (basic check)')
    parser.add_argument('--verbose', action='store_true', help='Verbose output')
    parser.add_argument('--stdout', action='store_true', help='Output to stdout instead of file')
    
    args = parser.parse_args()
    
    input_file = args.input
    
    if not os.path.exists(input_file):
        print(f"Error: File '{input_file}' not found")
        sys.exit(1)
    
    try:
        with open(input_file, 'r') as f:
            vsa_data = json.load(f)
    except json.JSONDecodeError as e:
        print(f"Error: Invalid JSON in '{input_file}': {e}")
        sys.exit(1)
    except Exception as e:
        print(f"Error reading file '{input_file}': {e}")
        sys.exit(1)
    
    try:
        isf_content = convert_vsa_to_isf(vsa_data, prune_builtins=not args.no_prune)
        
        
        # Basic shader validation
        if args.validate:
            shader_start = isf_content.find('*/') + 2
            shader_code = isf_content[shader_start:].strip()
            
            # Basic syntax checks
            issues = []
            if 'void main()' not in shader_code:
                issues.append("Missing main() function")
            if 'gl_Position' not in shader_code:
                issues.append("gl_Position not set (may cause rendering issues)")
            
            # Check for balanced braces
            open_braces = shader_code.count('{')
            close_braces = shader_code.count('}')
            if open_braces != close_braces:
                issues.append(f"Unbalanced braces: {open_braces} open, {close_braces} close")
            
            if issues:
                print("Validation warnings:")
                for issue in issues:
                    print(f"  - {issue}")
            else:
                print("Basic validation passed")
        
        # Output
        if args.stdout:
            print(isf_content)
        else:
            # Generate output filename
            if args.output:
                output_file = args.output
            else:
                # Use the sanitized VSA name from the JSON data
                vsa_name = vsa_data.get("name", "untitled")
                sanitized_name = sanitize_filename(vsa_name)
                output_file = get_unique_filename(sanitized_name)
            
            with open(output_file, 'w') as f:
                f.write(isf_content)
            
            if args.verbose:
                # Parse header for statistics
                header_start = isf_content.find('/*') + 2
                header_end = isf_content.find('*/')
                header_json = isf_content[header_start:header_end]
                header_data = json.loads(header_json)
                
                print(f"Converted '{input_file}' to '{output_file}'")
                print(f"  Title: {header_data.get('DESCRIPTION', 'Unknown')}")
                print(f"  Author: {header_data.get('CREDIT', 'Unknown').split()[0]}")
                print(f"  Point count: {header_data.get('POINT_COUNT', 0)}")
                print(f"  Mode: {header_data.get('PRIMITIVE_MODE', 'UNKNOWN')}")
                print(f"  Categories: {', '.join(header_data.get('CATEGORIES', []))}")
                print(f"  Inputs detected: {len(header_data.get('INPUTS', []))}")
            else:
                print(f"Converted '{input_file}' to '{output_file}'")
        
    except Exception as e:
        print(f"Error during conversion: {e}")
        if args.verbose:
            import traceback
            traceback.print_exc()
        sys.exit(1)

if __name__ == "__main__":
    main()
