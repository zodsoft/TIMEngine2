#version 430

#ifdef STEREO_DISPLAY
uniform sampler2D textures[2];
#else
	const int PORTAL_LIMIT = 2;
uniform sampler2D textures[2+PORTAL_LIMIT];
uniform int nbScene;
#endif

smooth in vec2 texc;

layout(location=0) out vec4 outColor0;

void main()
{
#ifdef STEREO_DISPLAY
	if(texc.x < 0.499) outColor0 = texture(textures[1], vec2(texc.x*2,texc.y));
	else if(texc.x > 0.501) outColor0 = texture(textures[0], vec2((texc.x-0.5)*2,texc.y));
	else outColor0 = vec4(1,0,0,1);
	
	//outColor0 = vec4(texc,0,1);
#else
	vec4 mat = texture(textures[0], texc); 
	outColor0 = texture(textures[1], texc);
	
	if(mat.x==1 && mat.y==1) 
	{
		for(int i=0 ; i<PORTAL_LIMIT ; ++i)
		{
			if(mat.z < 0.05 + i*0.1) 
			{
				outColor0 = texture(textures[i+2], texc);
				break;
			}
		}
	}
#endif

	outColor0.a = 1;
}