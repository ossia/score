#define CATCH_CONFIG_MAIN
#include "/home/jcelerier/score/3rdparty/libossia/tests/catch/catch.hpp"
#include <isf.hpp>

TEST_CASE ("texture test", "texture test")
{
  const char* frag = R"_(/*
                     {
                       "CATEGORIES" : [
                         "Geometry Adjustment", "Tile Effect"
                       ],
                       "DESCRIPTION" : "Repllcates a radial slice of an image",
                       "ISFVSN" : "2",
                       "INPUTS" : [
                         {
                           "NAME" : "inputImage",
                           "TYPE" : "image"
                         },
                         {
                           "NAME" : "postRotateAngle",
                           "TYPE" : "float",
                           "MAX" : 360,
                           "DEFAULT" : 0,
                           "LABEL" : "Post Rotate Angle",
                           "MIN" : 0
                         },
                         {
                           "NAME" : "numberOfDivisions",
                           "TYPE" : "float",
                           "MAX" : 360,
                           "DEFAULT" : 12,
                           "MIN" : 1,
                           "LABEL" : "Number Of Divisions"
                         },
                         {
                           "NAME" : "preRotateAngle",
                           "TYPE" : "float",
                           "MAX" : 180,
                           "DEFAULT" : 0,
                           "MIN" : -180,
                           "LABEL" : "Pre Rotate Angle"
                         },
                         {
                           "MIN" : 0,
                           "IDENTITY" : 0,
                           "DEFAULT" : 0,
                           "LABEL" : "Radius Start",
                           "TYPE" : "float",
                           "MAX" : 1,
                           "NAME" : "centerRadiusStart"
                         },
                         {
                           "MIN" : 0,
                           "IDENTITY" : 0,
                           "DEFAULT" : 1,
                           "LABEL" : "Radius End",
                           "TYPE" : "float",
                           "MAX" : 2,
                           "NAME" : "centerRadiusEnd"
                         }
                       ],
                       "CREDIT" : "VIDVOX"
                     }
                     */



                     const float pi = 3.14159265359;




                     void main()	{

                       vec4		inputPixelColor = vec4(0.0);
                       vec2		loc = _inputImage_imgRect.zw * vec2(isf_FragNormCoord.x,isf_FragNormCoord.y);
                       //	'r' is the radius- the distance in pixels from 'loc' to the center of the rendering space
                       //float		r = distance(IMG_SIZE(inputImage)/2.0, loc);
                       float		r = distance(_inputImage_imgRect.zw/2.0, loc);
                       //	'a' is the angle of the line segment from the center to loc is rotated
                       //float		a = atan ((loc.y-IMG_SIZE(inputImage).y/2.0),(loc.x-IMG_SIZE(inputImage).x/2.0));
                       float		a = atan ((loc.y-_inputImage_imgRect.w/2.0),(loc.x-_inputImage_imgRect.z/2.0));
                       float		modAngle = 2.0 * pi / numberOfDivisions;
                       float		scaledCenterRadiusStart = centerRadiusStart * max(RENDERSIZE.x,RENDERSIZE.y);
                       float		scaledCenterRadiusEnd = centerRadiusEnd * max(RENDERSIZE.x,RENDERSIZE.y);

                       if (scaledCenterRadiusStart > scaledCenterRadiusEnd)	{
                         scaledCenterRadiusStart = scaledCenterRadiusEnd;
                         scaledCenterRadiusEnd = centerRadiusStart * max(RENDERSIZE.x,RENDERSIZE.y);
                       }

                       if ((centerRadiusEnd != centerRadiusStart)&&(r >= scaledCenterRadiusStart)&&(r <= scaledCenterRadiusEnd))	{
                         r = (r - scaledCenterRadiusStart) / (centerRadiusEnd - centerRadiusStart);

                         a = mod(a + pi * postRotateAngle/360.0,modAngle);

                         //	now modify 'a', and convert the modified polar coords (radius/angle) back to cartesian coords (x/y pixels)
                         loc.x = r * cos(a + 2.0 * pi * (preRotateAngle) / 360.0);
                         loc.y = r * sin(a + 2.0 * pi * (preRotateAngle) / 360.0);

                         loc = loc / _inputImage_imgRect.zw + vec2(0.5);

                         if ((loc.x < 0.0)||(loc.y < 0.0)||(loc.x > 1.0)||(loc.y > 1.0))	{
                           inputPixelColor = vec4(0.0);
                         }
                         else	{
                           inputPixelColor = IMG_NORM_PIXEL(inputImage,loc);
                         }
                       }
                       gl_FragColor = inputPixelColor;
                     }
)_";
  isf::parser p{{}, frag};

  std::cerr << p.fragment() << std::endl;

}
