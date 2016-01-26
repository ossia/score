// The Flock (a list of Boid objects)
class Flock 
{
  ArrayList<Boid> boids; // An ArrayList for all the boids

  Flock() 
  {
    boids = new ArrayList<Boid>(); // Initialize the ArrayList
  }
  
  int getSize()
  {
    return boids.size();
  }

  void run() 
  {
    for (Boid b : boids) 
    {
      b.run(boids);  // Passing the entire list of boids to each boid individually
    }
  }

  void addBoid(Boid b) 
  {
    boids.add(b);
  }
  
  void setAvoidance(float avoidance) 
  {
    for (Boid b : boids) 
    {
      b.avoidance = avoidance;
    }
  }
  
  void setAlignment(float alignment) 
  {
    for (Boid b : boids) 
    {
      b.alignment = alignment;
    }
  }
  
  void setCohesion(float cohesion) 
  {
    for (Boid b : boids) 
    {
      b.cohesion = cohesion;
    }
  }
  
  void setDistanceMin(float distance_min) 
  {
    for (Boid b : boids) 
    {
      b.neighbor_distance_min = distance_min;
    }
  }
  
  void setDistanceMax(float distance_max) 
  {
    for (Boid b : boids) 
    {
      b.neighbor_distance_max = distance_max;
    }
  }
  
  void setFollowDestination(int x, int y)
  {
    for (Boid b : boids) 
    {
      b.destination = new PVector(x, y);
    }
  }
  
    void setFollowRate(float follow_rate)
  {
    for (Boid b : boids) 
    {
      b.follow_rate = follow_rate;
    }
  }
}

