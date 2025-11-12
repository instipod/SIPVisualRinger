#!/bin/bash

# Script to generate webpages.h from HTML files in the web folder

# Set the output file path
OUTPUT_FILE="../src/webpages.h"

# Start writing the header file
cat > "$OUTPUT_FILE" << 'HEADER_START'
// Auto-generated file - DO NOT EDIT
// Generated from HTML files in web/ folder

#ifndef WEBPAGES_H
#define WEBPAGES_H

HEADER_START

# Process each HTML file in the web directory
for html_file in *.html; do
    # Skip if no HTML files found
    if [ ! -f "$html_file" ]; then
        continue
    fi

    # Extract filename without extension
    filename="${html_file%.html}"

    # Create variable name (e.g., login.html -> webpage_login)
    var_name="webpage_${filename}"

    # Start the variable declaration with PROGMEM
    echo "const char ${var_name}[] PROGMEM = " >> "$OUTPUT_FILE"

    # Read the HTML file line by line and escape it
    while IFS= read -r line || [ -n "$line" ]; do
        # Escape backslashes first, then double quotes
        escaped_line="${line//\\/\\\\}"
        escaped_line="${escaped_line//\"/\\\"}"
        # Write the line as a C string
        echo "  \"${escaped_line}\\n\"" >> "$OUTPUT_FILE"
    done < "$html_file"

    # Close the string with semicolon
    echo ";" >> "$OUTPUT_FILE"
    echo "" >> "$OUTPUT_FILE"
done

# Process output.css file
css_file="output.css"
if [ -f "$css_file" ]; then
    # Create variable name for CSS
    var_name="css_output"

    # Start the variable declaration with PROGMEM
    echo "const char ${var_name}[] PROGMEM = " >> "$OUTPUT_FILE"

    # Read the CSS file line by line and escape it
    while IFS= read -r line || [ -n "$line" ]; do
        # Escape backslashes first, then double quotes
        escaped_line="${line//\\/\\\\}"
        escaped_line="${escaped_line//\"/\\\"}"
        # Write the line as a C string
        echo "  \"${escaped_line}\\n\"" >> "$OUTPUT_FILE"
    done < "$css_file"

    # Close the string with semicolon
    echo ";" >> "$OUTPUT_FILE"
    echo "" >> "$OUTPUT_FILE"
else
    echo "Warning: output.css not found in web directory"
fi

# Close the header guard
cat >> "$OUTPUT_FILE" << 'HEADER_END'
#endif // WEBPAGES_H
HEADER_END

echo "Generated $OUTPUT_FILE successfully"
