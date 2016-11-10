import oscP5.*;
import netP5.*;


PFont font;

OscP5 osc_in;
NetAddress osc_out;
class Target
{
  float x, y;
  public Target(float x1, float y1)
  {
    x = x1;
    y = y1;
  }
}

interface State
{
 public void draw();
 public void oscEvent(OscMessage osc_msg);
 public void mousePressed();
}

class IntroState implements State
{
  String maintext = "hello";
  int size = 30;
  int textX = 300;
  int textY = 300;
  
  float rectX() { return textX - 5; }
  float rectY() { return textY - size - 5; }
  float rectW() { return textWidth(maintext) + 10; }
  float rectH() { return size + 10; }
  
  public void draw()
  {
    textFont(font, size);
    
    fill(0);
    stroke(120);
    rect(rectX(), rectY(), rectW(), rectH());
    
    fill(255);
    
    text(maintext, textX, textY);
  }
  
  public void oscEvent(OscMessage osc_msg)
  {
    if (osc_msg.checkAddrPattern("/intro/size")) 
    {
      if (osc_msg.checkTypetag("i")) 
      { 
        size = osc_msg.get(0).intValue();
      }
    }
    else if (osc_msg.checkAddrPattern("/intro/text"))
    {
      if (osc_msg.checkTypetag("s")) 
      { 
        maintext = osc_msg.get(0).stringValue();
      }
    }
  }
  
  public void mousePressed()
  {
    if((mouseX > rectX()) && 
       (mouseX < rectX() + rectW()) && 
       (mouseY > rectY()) && 
       (mouseY < rectY() + rectH() ))
    {
      osc_in.send(new OscMessage("/main/touch"), osc_out);
    }
  }
}

class MainState implements State
{
  public int difficulty = 1;
  float bg_intensity = 0;
  ArrayList<Target> targets = new ArrayList<Target>();
  
  public MainState()
  {
    
  }
  public void draw()
  {
    // Add targets
    int r = int(random(100 - difficulty));
    if(r < 10)
    {
      targets.add(new Target(width/ 2 + random(200) - 100, height / 2 + random(200) + - 100));
    }
    
    fill(255);
    stroke(120);
    for(int i = 0; i < targets.size(); i++)
    {
      Target t = targets.get(i);
      
      ellipse(t.x, t.y, radius(), radius());
    }
    
    fill(255, 255, 255, bg_intensity * 255);
    rect(width/4, height/4, width/2, height/2);
    
  }
  
  public void oscEvent(OscMessage osc_msg)
  {if (osc_msg.checkAddrPattern("/bg_intensity")) 
    {
      if (osc_msg.checkTypetag("f")) 
      { 
        bg_intensity = osc_msg.get(0).floatValue();
      }
    }
  }
  
  private float radius()
  {
    return 50. / difficulty;
  }
  
  public void mousePressed()
  {
    for(int i = 0; i < targets.size(); i++)
    {
      Target t = targets.get(i);
      if(mouseX > t.x - radius() && mouseX < t.x + radius() && mouseY > t.y - radius() && mouseY < t.y + radius())
      {
        targets.remove(i);
        osc_in.send(new OscMessage("/main/touch"), osc_out);
      }
    }
  }
}

class SuccessState implements State
{
  public void draw()
  {
    fill(255, 155, 155);
    rect(width/4, height/4, width/2, height/2);
    fill(50, 255, 50);
    text("Win !", width/2 - textWidth("Win !") / 2, height/2);
  }
  
  public void oscEvent(OscMessage osc_msg)
  {
  }
  
  public void mousePressed()
  {
  }
}

class FailureState implements State
{
  public void draw()
  {
    fill(110, 50, 50);
    rect(width/4, height/4, width/2, height/2);
    fill(255, 50, 50);
    text("Fail !", width/2 - textWidth("Fail !") / 2, height/2);
  }
  
  public void oscEvent(OscMessage osc_msg)
  {
  }
  
  public void mousePressed()
  {
  }
}

State currentState;
IntroState intro;
MainState main;
SuccessState success;
FailureState failure;

void setup()
{
  size(1024, 768);
  
  // listen osc message from i-score
  osc_in = new OscP5(this, 13001);

  // output osc messages to i-score
  osc_out = new NetAddress("127.0.0.1", 13002);
  
  ellipseMode(CENTER);
  font = createFont("Helvetica", 16, true); 
  
  currentState = new IntroState();
}

void oscEvent(OscMessage osc_msg)
{
  if (osc_msg.checkAddrPattern("/state")) 
  {
    if (osc_msg.checkTypetag("s")) 
    { 
      String newState = osc_msg.get(0).stringValue();
      print(newState);
      if(newState.equals("intro"))
      {
        currentState = new IntroState();
      }
      else if(newState.equals("main"))
      {
        currentState = new MainState();
      }
      else if(newState.equals("success"))
      {
        currentState = new SuccessState();
      }
      else if(newState.equals("failure"))
      {
        currentState = new FailureState();
      }
    }
  }
  else if(osc_msg.checkAddrPattern("/main/difficulty"))
  {
    if (osc_msg.checkTypetag("i")) 
    { 
      main.difficulty = osc_msg.get(0).intValue();
    }
  }
  
  currentState.oscEvent(osc_msg);
}

void draw()
{
  fill(120, 1);
  background(20);
  
  currentState.draw();
}

void mousePressed()
{ 
  currentState.mousePressed();
}

void mouseMoved()
{
}
void mouseReleased()
{
}