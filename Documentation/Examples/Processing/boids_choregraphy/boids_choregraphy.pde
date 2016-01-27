/**
 * boids_choregraphy
 * based on the Daniel Shiffman's Flocking example.
 * 
 * An implementation of Craig Reynold's Boids program to simulate
 * the flocking behavior of birds. Each boid steers itself based on 
 * rules of avoidance, alignment and cohesion.
 * 
 * Control the sketch using keyboard () or via OSC messages:
 *
 * (i) /flock/info i : display flock parameters (default true)
 * (c) /flock/clear : delete all existing boids
 * (a) /flock/add f f : message to add a new boid at a position
 * (-) /flock/avoidance f : parameter to control avoidance weight (default 1.)
 * (-) /flock/alignment f : parameter to control alignment weight (default 1.)
 * (-) /flock/cohesion f : parameter to control cohesion weight (default 1.)
 * (-) /flock/distance/min f : parameter to control minimal distance between boids (default 50.)
 * (-) /flock/distance/max f : parameter to control maximal distance between boids (default 75.)
 * (f) /flock/follow/destination i i : parameter to make boids to follow a destination (default 0. 0.)
 * (-) /flock/follow/rate f : parameter to control how much boids follow the destination (default 0.)
 * (-) /mouse/x i : returns mouse x position on screen
 * (-) /mouse/y i : returns mouse y position on screen
 * (-) /mouse/click : returns 1 when mouse is pressed and 0 when released
 */

import oscP5.*;
import netP5.*;

OscP5 osc_in;
NetAddress osc_out;

Flock flock;

int last_mouse_x;    // to filter repetitions
int last_mouse_y;    // to filter repetitions
int last_flock_size; // to filter repetitions
 
boolean info = true;

void setup() 
{
  size(640, 360);

  // listen osc message from i-score
  osc_in = new OscP5(this, 12001);

  // output osc messages to i-score
  osc_out = new NetAddress("127.0.0.1", 12002);

  flock = new Flock();
}

void draw()
{
  OscMessage osc_msg;
  
  background(50);

  // update flock
  flock.run();

  // draw flock informations
  if (info)
    flock.draw_info();
    
  // send OSC message for flock size
  osc_msg = new OscMessage("/flock/size");
  osc_msg.add(flock.getSize());
  osc_in.send(osc_msg, osc_out);

  // send OSC message for flock deviation
  osc_msg = new OscMessage("/flock/deviation");
  osc_msg.add(flock.getDeviation().x);
  osc_msg.add(flock.getDeviation().y);
  osc_in.send(osc_msg, osc_out);

  // send OSC message for mouseX
  if (mouseX != last_mouse_x)
  {
    last_mouse_x = mouseX;

    osc_msg = new OscMessage("/mouse/x");
    osc_msg.add(mouseX);
    osc_in.send(osc_msg, osc_out);
  }

  // send OSC message for mouseY
  if (mouseY != last_mouse_y)
  {
    last_mouse_y = mouseY;

    osc_msg = new OscMessage("/mouse/y");
    osc_msg.add(mouseY);
    osc_in.send(osc_msg, osc_out);
  }
  
  // use keyboard to control flock parameters
  if (keyPressed)
  {
    switch (key)
    {
    case 'f':
      {
        flock.setFollowDestination(mouseX, mouseY);
        break;
      }
    };
  }
}

void mousePressed()
{
  OscMessage osc_msg = new OscMessage("/mouse/click");
  osc_msg.add(1);
  osc_in.send(osc_msg, osc_out);
}

void mouseReleased()
{
  OscMessage osc_msg = new OscMessage("/mouse/click");
  osc_msg.add(0);
  osc_in.send(osc_msg, osc_out);
}

void oscEvent(OscMessage osc_msg)
{
  // debug
  println(osc_msg.addrPattern() + "[" + osc_msg.typetag() + "]");

  if (osc_msg.checkAddrPattern("/flock/info")) 
  {
    if (osc_msg.checkTypetag("i")) 
    {
      info = osc_msg.get(0).intValue() == 1;
    }
  } else if (osc_msg.checkAddrPattern("/flock/clear")) 
  {
    flock.clear();
  } else if (osc_msg.checkAddrPattern("/flock/add")) 
  {
    if (osc_msg.checkTypetag("ff")) 
    {
      float x = osc_msg.get(0).floatValue();
      float y = osc_msg.get(1).floatValue();
      flock.addBoid(new Boid(x, y));
    }
  } else if (osc_msg.checkAddrPattern("/flock/avoidance"))
  {
    if (osc_msg.checkTypetag("f")) 
    {
      float avoidance = osc_msg.get(0).floatValue();
      flock.setAvoidance(avoidance);
    }
  } else if (osc_msg.checkAddrPattern("/flock/alignment"))
  {
    if (osc_msg.checkTypetag("f")) 
    {
      float alignment = osc_msg.get(0).floatValue();
      flock.setAlignment(alignment);
    }
  } else if (osc_msg.checkAddrPattern("/flock/cohesion"))
  {
    if (osc_msg.checkTypetag("f")) 
    {
      float cohesion = osc_msg.get(0).floatValue();
      flock.setCohesion(cohesion);
    }
  } else if (osc_msg.checkAddrPattern("/flock/distance/min"))
  {
    if (osc_msg.checkTypetag("f")) 
    {
      float distance_min = osc_msg.get(0).floatValue();
      flock.setDistanceMin(distance_min);
    }
  } else if (osc_msg.checkAddrPattern("/flock/distance/max"))
  {
    if (osc_msg.checkTypetag("f")) 
    {
      float distance_max = osc_msg.get(0).floatValue();
      flock.setDistanceMax(distance_max);
    }
  } else  if (osc_msg.checkAddrPattern("/flock/follow/destination")) 
  {
    if (osc_msg.checkTypetag("ii")) 
    {
      int x = osc_msg.get(0).intValue();
      int y = osc_msg.get(1).intValue();
      flock.setFollowDestination(x, y);
    }
  } else  if (osc_msg.checkAddrPattern("/flock/follow/rate")) 
  {
    if (osc_msg.checkTypetag("f")) 
    {
      float follow_rate = osc_msg.get(0).floatValue();
      flock.setFollowRate(follow_rate);
    }
  }
}

void keyPressed()
{
  switch (key)
  {
  case 'i':
    {
      info = !info;
      break;
    }
  case 'c':
    {
      flock.clear();
      break;
    }
  case 'a':
    {
      flock.addBoid(new Boid(mouseX, mouseY));
      break;
    }
  case 'f':
    {
      noCursor();
      flock.setFollowRate(1.0);
      break;
    }
  };
}

void keyReleased()
{
  switch (key)
  {
  case 'f':
    {
      cursor();
      flock.setFollowRate(0.0);
      break;
    }
  };
}

