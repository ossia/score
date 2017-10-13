using UnityEngine;
using System.Collections;
using Ossia;

public class ExampleAttribute : MonoBehaviour {

	// Attributes in libossia can be exposed this way : 
	[Ossia.Expose("sympa")]
	[Range (0, 100)]
	public int Sympa;

  [Ossia.Expose("test")]
  public Vector3 MyProperty { get; set; }

	// Use this for initialization
	void Start () {

	}

	// Update is called once per frame
	void Update () {

	}
}
