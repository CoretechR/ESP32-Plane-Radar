import argparse
from PIL import Image
import os

def convert_image_to_uint8_array(image_path, array_name):
    # Check if file exists
    if not os.path.exists(image_path):
        print(f"Error: Could not find '{image_path}'")
        return

    try:
        # Open the image
        img = Image.open(image_path)
        img = img.convert("RGBA")
        
        sprite_size = 15
        
        # 1. Scale down if the image is larger than 15x15, preserving aspect ratio.
        img.thumbnail((sprite_size, sprite_size), Image.Resampling.NEAREST)
        
        # 2. Create a blank, transparent 15x15 canvas
        canvas = Image.new("RGBA", (sprite_size, sprite_size), (0, 0, 0, 0))
        
        # 3. Calculate center offsets and paste the image onto the canvas
        offset_x = (sprite_size - img.width) // 2
        offset_y = (sprite_size - img.height) // 2
        canvas.paste(img, (offset_x, offset_y))
        
        pixels = canvas.getdata()
        
        # Generate the C++ code
        print(f"const uint8_t k{array_name}[kAirlinerSpriteSize][kAirlinerSpriteSize] PROGMEM = {{")
        
        # Process the 15x15 canvas row by row
        for y in range(sprite_size):
            row_vals = []
            for x in range(sprite_size):
                # Extract RGBA values for the current pixel
                r, g, b, a = pixels[y * sprite_size + x]
                
                # Calculate perceptual luminance (0 = Black, 255 = White)
                luminance = 0.299 * r + 0.587 * g + 0.114 * b
                
                # Calculate "darkness" (inverted luminance)
                darkness = 255.0 - luminance
                
                # Multiply by alpha (normalized to 0-1) to get the final effective intensity.
                effective_intensity = int(round((a / 255.0) * darkness))
                
                # Clamp the value between 0 and 255 just to be perfectly safe
                effective_intensity = max(0, min(255, effective_intensity))
                
                row_vals.append(str(effective_intensity))
            
            # Join the row into a comma-separated string and print it inside curly braces
            row_string = ", ".join(row_vals)
            print(f"    {{{row_string}}},")
            
        print("};")
        
    except Exception as e:
        print(f"An error occurred: {e}")

if __name__ == "__main__":
    # Set up the command-line argument parser
    parser = argparse.ArgumentParser(description="Convert an image to a 2D C++ uint8_t array.")
    
    # Required argument: The image file path
    parser.add_argument("image", help="Path to the input image file (e.g., turboprop.png)")
    
    # Optional argument: The name of the array (defaults to SpriteTurboprop)
    parser.add_argument("--name", default="SpriteTurboprop", help="Name of the generated array")
    
    # Parse the arguments provided by the user
    args = parser.parse_args()
    
    # Run the converter with the specified arguments
    convert_image_to_uint8_array(args.image, args.name)