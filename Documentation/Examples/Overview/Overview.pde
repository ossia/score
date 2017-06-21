import oscP5.*;
import netP5.*;
OscP5 osc_in;

float radius = 40;

int numCircles = 10;
float xpos = 320;
float ypos = 240;

color c = color(150, 150, 150);


// Called once at the opening
void setup()
{
  // Setup the window size
  size(1024, 768);
  
  // Enable Processing to receive OSC messages
  osc_in = new OscP5(this, 9996);
}

// Called every time an OSC message is received
void oscEvent(OscMessage osc_msg)
{
 
  // We are looking for a "/radius" message with a single floating point value.
  if (osc_msg.checkAddrPattern("/my_float")) 
  {
      radius = osc_msg.get(0).floatValue();
  }
  else if (osc_msg.checkAddrPattern("/my_int")) 
  {
      numCircles = osc_msg.get(0).intValue();
  }
  else if (osc_msg.checkAddrPattern("/my_pos")) 
  {
      xpos = 320 + 500 * osc_msg.get(0).floatValue();
      ypos = 240 + 500 * osc_msg.get(1).floatValue();
  }
  else if (osc_msg.checkAddrPattern("/my_color")) 
  {
      float r = osc_msg.get(0).floatValue() * 255;
      float g = osc_msg.get(1).floatValue() * 255;
      float b = osc_msg.get(2).floatValue() * 255;
      c = color(r,g,b);
  }
}

float time = 0.;
// Called when the screen must be refreshed.
void draw()
{
  // Reset the background
  background(30);
  fill(c);
  
  float theta = 0.;
  float r = radius * 3.;
  for(int i = 0; i < numCircles; i++)
  {
    float x = xpos + r * cos(theta + time);
    float y = ypos + r * sin(theta + time);
    ellipse(x, y, radius, radius);
    theta += 2. * PI / numCircles;
  }
  
  time += 0.01;
}