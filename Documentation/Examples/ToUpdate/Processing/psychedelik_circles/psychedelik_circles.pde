/**
 * psychedelik_jungle by Théo de la Hogue
 * a simple graphical sketch to illustrate 
 * how i-score can interact with Processing
 */

import oscP5.*;
import netP5.*;

OscP5 osc_in;
NetAddress osc_out;

int nbCircles = 20;
float[] size;
float speed = 0.1;

void setup() 
{
  size(500, 500);

  // listen osc message from i-score
  osc_in = new OscP5(this, 12001);

  // output osc messages to i-score
  osc_out = new NetAddress("127.0.0.1", 12002);

  // initialisation for psychdelik pattern
  noFill();
  size = new float[nbCircles];
}

void draw() 
{
  background(0);

  for (int i=0; i<nbCircles; i++) 
  {  
    size[i] += i*speed;
    if (size[i] < 0)
      size[i] = width;
    else if (size[i] > width)
     size[i] = 0;

    stroke(250 - size[i]/2);
    ellipse(250, 250, size[i], size[i]);
  }
}

void mousePressed() 
{
  OscMessage osc_msg = new OscMessage("/click");
  osc_in.send(osc_msg, osc_out);
}

void oscEvent(OscMessage osc_msg) 
{
  if (osc_msg.checkAddrPattern("/number")) 
  {
    if (osc_msg.checkTypetag("i")) 
    {
      // debug
      println(osc_msg.addrPattern() + "[" + osc_msg.typetag() + "]");

      nbCircles = osc_msg.get(0).intValue();
      size = new float[nbCircles];
      for (int i=0; i<nbCircles; i++)
        size[i] = 0;
    }
  } else if (osc_msg.checkAddrPattern("/speed"))
  {
    if (osc_msg.checkTypetag("f")) 
    {
      speed = osc_msg.get(0).floatValue();
    }
  }
}

