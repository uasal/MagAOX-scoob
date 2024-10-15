#!/bin/bash

# Define the device and output JSON file
DEVICE="/dev/video2"
JSON_FILE="camera_info.json"

# Run v4l2-ctl command and capture output
v4l2_output=$(v4l2-ctl -d "$DEVICE" --all)

# Initialize JSON
json="{\"device\": \"$DEVICE\", "

# Parameters to include
parameters=("Width/Height" "Pixel Format" "Field" "Bytes per Line" "Size Image" "Colorspace" "Transfer Function" "YCbCr/HSV Encoding" "Quantization" "sensor_mode" "gain" "exposure" "frame_rate" "low_latency_mode" "blacklevel")

# Function to parse and format nested properties
parse_nested() {
    local line="$1"
    local key="${line%%=*}"
    local value="${line#*=}"
    local result="\"$key\":\"$value\""
    echo "$result"
}

# Add parameters to JSON
for param in "${parameters[@]}"; do
    if [[ "$param" == "gain" || "$param" == "exposure" || "$param" == "frame_rate" || "$param" == "low_latency_mode" || "$param" == "blacklevel" || "$param" == "sensor_mode" ]]; then
        value=$(echo "$v4l2_output" | grep "$param" | tail -n1 | sed 's/^[[:space:]]*[^:]*:[[:space:]]*\(.*\)/\1/')
        nested_json="{"
        IFS=', ' read -r -a entries <<< "$(echo "$value" | tr ' ' ',')"
        for entry in "${entries[@]}"; do
            nested_json+=$(parse_nested "$entry"), 
        done
        nested_json=$(echo "$nested_json" | sed 's/,$//')
        nested_json+="}"
        json+="\"$param\": $nested_json,"
    else
        value=$(echo "$v4l2_output" | grep "$param" | tail -n1 | sed 's/^[[:space:]]*[^:]*:[[:space:]]*\(.*\)/\1/')
        json+="\"$param\": \"$value\","
    fi
done

# Remove trailing comma and close JSON object
json=$(echo "$json" | sed 's/,$//')
json+="}"

# Write JSON to file
echo "$json" > "$JSON_FILE"

echo "JSON file created: $JSON_FILE"

