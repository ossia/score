/**
 * boids_choregraphy
 * based on the Daniel Shiffman's Flocking example.
 * 
 * An implementation of Craig Reynold's Boids program to simulate
 * the flocking behavior of birds. Each boid steers itself based on 
 * rules of avoidance, alignment, and cohesion.
 * 
 * Use i-score control the sketch via OSC:
 *
 * - /flock/add i i : message to add a new boid at a position
 * - /flock/avoidance f : parameter to control avoidance weight (default 1.)
 * - /flock/alignment f : parameter to control alignment weight (default 1.)
 * - /flock/cohesion f : parameter to control cohesion weight (default 1.)
 * - /flock/distance/min f : parameter to control minimal distance between boids (default 25.)
 * - /flock/distance/max f : parameter to control maximal distance between boids (default 50.)
 * - /flock/follow/destination i i : parameter to make boids to follow a destination (default 0. 0.)
 * - /flock/follow/rate f : parameter to control how much boids follow the destination (default 0.)
 * - /mouse/x i : returns mouse x position on screen
 * - /mouse/y i : returns mouse y position on screen
 * - /mouse/pressed : returned when mouse is pressed
 * - /mouse/released : returned when mouse is released
 */
 
import oscP5.*;
import netP5.*;

OscP5 osc_in;
NetAddress osc_out;

Flock flock;

int last_mouse_x;  // to filter repetitions
int last_mouse_y;  // to filter repetitions

void setup() 
{
  size(640, 360);
  
  // listen osc message from i-score
  osc_in = new OscP5(this, 12001);

  // output osc messages to i-score
  osc_out = new NetAddress("127.0.0.1", 12002);
  
  flock = new Flock();
  
  for (int i = 0; i < 50 ; i++)
    flock.addBoid(new Boid(width/2, height/2));
}

void draw()
{
  background(50);
  flock.run();
  
  if (mouseX != last_mouse_x)
  {
    last_mouse_x = mouseX;
    
    OscMessage osc_msg = new OscMessage("/mouse/x");
    osc_msg.add(mouseX);
    osc_in.send(osc_msg, osc_out);
  }
  
  if (mouseY != last_mouse_y)
  {
    last_mouse_y = mouseY;
    
    OscMessage osc_msg = new OscMessage("/mouse/y");
    osc_msg.add(mouseY);
    osc_in.send(osc_msg, osc_out);
  }
}

void mousePressed()
{
  OscMessage osc_msg = new OscMessage("/mouse/pressed");
  osc_in.send(osc_msg, osc_out);
}

void mouseReleased()
{
  OscMessage osc_msg = new OscMessage("/mouse/released");
  osc_in.send(osc_msg, osc_out);
}

void oscEvent(OscMessage osc_msg)
{
  if (osc_msg.checkAddrPattern("/flock/add")) 
  {
    if (osc_msg.checkTypetag("ii")) 
    {
      // debug
      println(osc_msg.addrPattern() + "[" + osc_msg.typetag() + "]");

      int x = osc_msg.get(0).intValue();
      int y = osc_msg.get(1).intValue();
      flock.addBoid(new Boid(x, y));
    }
  }
  else if (osc_msg.checkAddrPattern("/flock/avoidance"))
  {
    if (osc_msg.checkTypetag("f")) 
    {
      float avoidance = osc_msg.get(0).floatValue();
      flock.setAvoidance(avoidance);
    }
  }
  else if (osc_msg.checkAddrPattern("/flock/alignment"))
  {
    if (osc_msg.checkTypetag("f")) 
    {
      float alignment = osc_msg.get(0).floatValue();
      flock.setAlignement(alignment);
    }
  }
  else if (osc_msg.checkAddrPattern("/flock/cohesion"))
  {
    if (osc_msg.checkTypetag("f")) 
    {
      float cohesion = osc_msg.get(0).floatValue();
      flock.setCohesion(cohesion);
    }
  }
  else if (osc_msg.checkAddrPattern("/flock/distance/min"))
  {
    if (osc_msg.checkTypetag("f")) 
    {
      float distance_min = osc_msg.get(0).floatValue();
      flock.setDistanceMin(distance_min);
    }
  }
  else if (osc_msg.checkAddrPattern("/flock/distance/max"))
  {
    if (osc_msg.checkTypetag("f")) 
    {
      float distance_max = osc_msg.get(0).floatValue();
      flock.setDistanceMax(distance_max);
    }
  }
  else  if (osc_msg.checkAddrPattern("/flock/follow/destination")) 
  {
    if (osc_msg.checkTypetag("ii")) 
    {
      // debug
      println(osc_msg.addrPattern() + "[" + osc_msg.typetag() + "]");

      int x = osc_msg.get(0).intValue();
      int y = osc_msg.get(1).intValue();
      flock.setFollowDestination(x, y);
    }
  }
  else  if (osc_msg.checkAddrPattern("/flock/follow/rate")) 
  {
    if (osc_msg.checkTypetag("f")) 
    {
      float follow_rate = osc_msg.get(0).floatValue();
      flock.setFollowRate(follow_rate);
    }
  }
}
