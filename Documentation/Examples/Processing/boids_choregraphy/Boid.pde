// The Boid class
class Boid 
{
  boolean alive = true;
  boolean out = false;
  boolean kill_out = false;
  boolean collision = false;
  
  PVector location;
  PVector velocity;
  PVector acceleration;
  PVector follow_destination;
  
  float size;
  float maxforce;    // Maximum steering force
  float maxspeed;    // Maximum speed
  
  float avoidance = 1.0;
  float alignment = 1.0;
  float cohesion = 1.0;
  float follow_rate = 0.0;
  
  float neighbor_distance_min = 50.0;
  float neighbor_distance_max = 75.0;
  
  Boid(float x, float y) 
  {
    acceleration = new PVector(0, 0);
    follow_destination = new PVector(0, 0);

    // This is a new PVector method not yet implemented in JS
    // velocity = PVector.random2D();

    // Leaving the code temporarily this way so that this example runs in JS
    float angle = random(TWO_PI);
    velocity = new PVector(cos(angle), sin(angle));

    location = new PVector(x, y);
    size = 3.0;
    maxspeed = 2;
    maxforce = 0.03;
  }

  void run(ArrayList<Boid> boids) 
  {
    if (!alive)
      return;
      
    flock(boids);
    update();
    borders();
    
    if (out && kill_out)
    {
        alive = false;
        return;
    }
        
    render();
  }
  
  // We accumulate a new acceleration each time based on three rules
  void flock(ArrayList<Boid> boids) 
  {
    // reset acceleration, out and collision
    acceleration.mult(0);
    out = false;
    collision = false;
    
    PVector avo = avoid(boids);       // Avoidance
    PVector ali = align(boids);       // Alignment
    PVector coh = cohesion(boids);    // Cohesion
    PVector fol = follow();           // Follower
    
    // weight those forces
    avo.mult(avoidance);
    ali.mult(alignment);
    coh.mult(cohesion);
    fol.mult(follow_rate);
    
    // Add the force vectors to acceleration
    acceleration.add(avo);
    acceleration.add(ali);
    acceleration.add(coh);
    acceleration.add(fol);
  }

  // Method to update location
  void update() 
  {
    // Update velocity
    velocity.add(acceleration);
    
    // Limit speed
    velocity.limit(maxspeed);
    location.add(velocity);
  }

  // A method that calculates and applies a steering force towards a target
  // STEER = DESIRED MINUS VELOCITY
  PVector seek(PVector target) 
  {
    PVector desired = PVector.sub(target, location);  // A vector pointing from the location to the target
    // Scale to maximum speed
    desired.normalize();
    desired.mult(maxspeed);

    // Above two lines of code below could be condensed with new PVector setMag() method
    // Not using this method until Processing.js catches up desired.setMag(maxspeed);

    // Steering = Desired minus Velocity
    PVector steer = PVector.sub(desired, velocity);
    steer.limit(maxforce);  // Limit to maximum steering force
    return steer;
  }

  void render() 
  {
    // Draw a triangle rotated in the direction of velocity
    float theta = velocity.heading2D() + radians(90);
    // heading2D() above is now heading() but leaving old syntax until Processing.js catches up

    if (collision)
    {
      fill(200, 100, 100);
      stroke(200, 100, 100);
    }
    else
    {
      fill(100, 200, 100);
      stroke(100, 200, 100);
    }
      
    pushMatrix();
    translate(location.x, location.y);
    rotate(theta);
    beginShape(TRIANGLES);
    vertex(0, -size*2);
    vertex(-size, size*2);
    vertex(size, size*2);
    endShape();
    popMatrix();
  }

  // Wraparound
  void borders() 
  {
    if (location.x < -10.*size) 
    {
      velocity.x = -1.*velocity.x;
      out = true;
    }
    
    if (location.y < -10.*size) 
    {
      velocity.y = -1.*velocity.y;
      out = true;
    }
    
    if (location.x > width + 10.*size)
    {
      velocity.x = -1.*velocity.x;
      out = true;
    }
    
    if (location.y > height + 10.*size)
    {
      velocity.y = -1.*velocity.y;
      out = true;
    }
  }

  // Avoidance
  // Method checks for nearby boids and steers away
  PVector avoid(ArrayList<Boid> boids) 
  {
    PVector steer = new PVector(0, 0, 0);
    int count = 0;
    
    // For every boid in the system, check if it's too close
    for (Boid other : boids) 
    {
      if (!other.alive)
        continue;
        
      float d = PVector.dist(location, other.location);
      
      // If the distance is greater than 0 and less than a minimal distance (0 when you are yourself)
      if ((d > 0) && (d < neighbor_distance_min)) 
      {
        // Calculate vector pointing away from neighbor
        PVector diff = PVector.sub(location, other.location);
        
        if (diff.mag() < size/2. && !out && !other.out)
          collision = true;
        
        diff.normalize();
        diff.div(d);        // Weight by distance
        steer.add(diff);
        count++;            // Keep track of how many
      }
    }
    
    // Average -- divide by how many
    if (count > 0) 
    {
      steer.div((float)count);
    }

    // As long as the vector is greater than 0
    if (steer.mag() > 0) 
    {
      // First two lines of code below could be condensed with new PVector setMag() method
      // Not using this method until Processing.js catches up
      // steer.setMag(maxspeed);

      // Implement Reynolds: Steering = Desired - Velocity
      steer.normalize();
      steer.mult(maxspeed);
      steer.sub(velocity);
      steer.limit(maxforce);
    }
    
    return steer;
  }

  // Alignment
  // For every nearby boid in the system, calculate the average velocity
  PVector align(ArrayList<Boid> boids) 
  {
    PVector sum = new PVector(0, 0);
    int count = 0;
    
    for (Boid other : boids) 
    {
      if (!other.alive)
        continue;
        
      float d = PVector.dist(location, other.location);
      if ((d > 0) && (d < neighbor_distance_max)) 
      {
        sum.add(other.velocity);
        count++;
      }
    }
    
    if (count > 0) 
    {
      sum.div((float)count);
      // First two lines of code below could be condensed with new PVector setMag() method
      // Not using this method until Processing.js catches up
      // sum.setMag(maxspeed);

      // Implement Reynolds: Steering = Desired - Velocity
      sum.normalize();
      sum.mult(maxspeed);
      PVector steer = PVector.sub(sum, velocity);
      steer.limit(maxforce);
      return steer;
    }
    else 
    {
      return new PVector(0, 0);
    }
  }

  // Cohesion
  // For the average location (i.e. center) of all nearby boids, calculate steering vector towards that location
  PVector cohesion(ArrayList<Boid> boids) 
  {
    PVector sum = new PVector(0, 0);   // Start with empty vector to accumulate all locations
    int count = 0;
    
    for (Boid other : boids) 
    {
      if (!other.alive)
        continue;
        
      float d = PVector.dist(location, other.location);
      if ((d > 0) && (d < neighbor_distance_max)) 
      {
        sum.add(other.location); // Add location
        count++;
      }
    }
    
    if (count > 0) 
    {
      sum.div(count);
      return seek(sum);  // Steer towards the location
    } 
    else 
    {
      return new PVector(0, 0);
    }
  }
  
  // Follow
  PVector follow()
  {
    return seek(follow_destination);
  }
}
