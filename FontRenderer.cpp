#include "FontRenderer.h"

#include <map>

#include "ethnocentric_places.h"

#include "glyph_fx.h"

#include "ScreenSpaceFxProcessor.h"

/************************************************************************/
/* 
Font used here is Ehtnocentric free font:
http://www.dafont.com/ethnocentric.font
Font atlas is generated by modifiend Microsoft Font Maker (originally made for XNA, hence the horrible pink spacing color):
http://xbox.create.msdn.com/en-US/education/catalog/utility/bitmap_font_maker
modified version (that generates font header file with placemarks) in 3DParty/BitmapFontMaker_4_0_mod.zip
*/
/************************************************************************/


CFontRenderer::CFontRenderer( LPWSTR fontAtlasResource, ID3D10Device * device )
{
	CComPtr<ID3D10Texture2D> tex;
	D3D10_TEXTURE2D_DESC srvd;


	if(FAILED(D3DX10CreateShaderResourceViewFromResource( device,0, fontAtlasResource, nullptr, nullptr, &fontAtlas.p, nullptr )))
		return;   
	
	fontAtlas->GetResource((ID3D10Resource**)&tex.p);

	tex->GetDesc(&srvd);

	fontAtlasSize = D3DXVECTOR2(srvd.Width,srvd.Height);


	DWORD dwShaderFlags = D3D10_SHADER_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
	dwShaderFlags |= D3D10_SHADER_DEBUG;
#endif

	auto hr = D3DX10CreateEffectFromMemory(glyph_fx_compiled,sizeof(glyph_fx_compiled),nullptr,nullptr,nullptr,"fx_4_0",dwShaderFlags,0,device,nullptr,nullptr,&fxGlyph.p,nullptr,nullptr);
	if( FAILED( hr ) )
	{
		MessageBox( NULL,L"Could not load Glyph shader.", L"Error", MB_OK );
		return;
	}

	// Define the input layout 
	D3D10_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 4*4, D3D10_INPUT_PER_VERTEX_DATA, 0 },
	};
	UINT numElements = sizeof( layout ) / sizeof( layout[0] );

	// Create the input layout
	D3D10_PASS_DESC PassDesc;
	fxGlyph->GetTechniqueByIndex(0)->GetPassByIndex( 0 )->GetDesc( &PassDesc );
	hr = device->CreateInputLayout( layout, numElements, PassDesc.pIAInputSignature,
		PassDesc.IAInputSignatureSize, &vertex3Layout );
	if( FAILED( hr ) )
		return;

	///quad

	Vertex3D vtxData[] = 
	{
		Vertex3D(D3DXVECTOR4(-1,-1,0.5,1),D3DXVECTOR2(0,1)),
		Vertex3D(D3DXVECTOR4(1,-1,0.5,1),D3DXVECTOR2(1,1)),
		Vertex3D(D3DXVECTOR4(-1,1,0.5,1),D3DXVECTOR2(0,0)),
		Vertex3D(D3DXVECTOR4(1,1,0.5,1),D3DXVECTOR2(1,0)),
	};

	D3D10_BUFFER_DESC bufferDesc;
	ZeroMemory(&bufferDesc,sizeof(bufferDesc));
	bufferDesc.Usage = D3D10_USAGE_DYNAMIC;
	bufferDesc.ByteWidth = sizeof( vtxData );
	bufferDesc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
	bufferDesc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
	bufferDesc.MiscFlags = 0;

	D3D10_SUBRESOURCE_DATA InitData;
	ZeroMemory(&InitData,sizeof(InitData));
	InitData.pSysMem = vtxData;
	hr = device->CreateBuffer( &bufferDesc, &InitData, &fsGlyVtxBuf );
	if( FAILED( hr ) )
		return;
}



CFontRenderer::~CFontRenderer(void)
{

}

D3DXVECTOR2 CFontRenderer::RenderGlyph( ID3D10Device * device,const D3DXVECTOR2 & pos, wchar_t glyph, D3DXVECTOR4 color)
{

	ID3D10EffectTechnique * tech = fxGlyph->GetTechniqueByName("Render");

	auto gly = Ethnocentric_Rg_glyphs.find(glyph);

	if(gly == Ethnocentric_Rg_glyphs.end())
		gly = Ethnocentric_Rg_glyphs.begin();

	D3D10_VIEWPORT srcViewportDesc;
	UINT nVpt = 1;

	device->RSGetViewports( &nVpt, &srcViewportDesc );
	
	D3DXVECTOR2 glySrc_TL(gly->second.x,gly->second.y);
	D3DXVECTOR2 glyDimPx = D3DXVECTOR2(gly->second.width,gly->second.height);
	D3DXVECTOR2 glySrc_RB = glySrc_TL + glyDimPx;

	D3DXVECTOR2 glyTL(pos.x,pos.y);
	D3DXVECTOR2 glyRB = glyTL + glyDimPx;

	Vertex3D * vtx;

	if(SUCCEEDED(fsGlyVtxBuf->Map(D3D10_MAP_WRITE_DISCARD,0,(void**)&vtx)))
	{
		vtx[0].Tex = D3DXVECTOR2(glySrc_TL.x, glySrc_TL.y);
		vtx[1].Tex = D3DXVECTOR2(glySrc_RB.x,glySrc_TL.y);
		vtx[2].Tex = D3DXVECTOR2(glySrc_TL.x,glySrc_RB.y);
		vtx[3].Tex = D3DXVECTOR2(glySrc_RB.x, glySrc_RB.y);

		vtx[0].Pos = D3DXVECTOR4(glyTL.x,glyTL.y,0.5,1);
		vtx[1].Pos = D3DXVECTOR4(glyRB.x,glyTL.y,0.5,1);
		vtx[2].Pos = D3DXVECTOR4(glyTL.x,glyRB.y,0.5,1);
		vtx[3].Pos = D3DXVECTOR4(glyRB.x,glyRB.y,0.5,1);
		
		fsGlyVtxBuf->Unmap();
	}

	UINT stride = sizeof( Vertex3D );
	UINT offset = 0;

	fxGlyph->GetVariableByName("srcTexture")->AsShaderResource()->SetResource(fontAtlas);

	fxGlyph->GetVariableByName("fontAtlasSize")->AsVector()->SetFloatVector(fontAtlasSize);
	fxGlyph->GetVariableByName("srcViewportSize")->AsVector()->SetFloatVector(D3DXVECTOR2(srcViewportDesc.Width,srcViewportDesc.Height));
	fxGlyph->GetVariableByName("color")->AsVector()->SetFloatVector((float*)color);

	device->IASetInputLayout(vertex3Layout);
	device->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	device->IASetVertexBuffers( 0, 1, &fsGlyVtxBuf.p, &stride, &offset );
	tech->GetPassByIndex( 0 )->Apply( 0 );
	device->Draw(4,0);

	return D3DXVECTOR2(pos.x + gly->second.width,pos.y);
}

D3DXVECTOR2 CFontRenderer::RenderString(ID3D10Device * device,std::wstring text, D3DXVECTOR2 pos, const D3DXVECTOR4 & color)
{
	for (auto & c : text)
		pos = RenderGlyph(device,pos,c,color);

	return pos;
}

D3DXVECTOR2 CFontRenderer::MeasureString( std::wstring text )
{
	D3DXVECTOR2 rt = D3DXVECTOR2(0,0);

	for (auto & c : text)
	{
		auto gly = Ethnocentric_Rg_glyphs.find(c);

		if(gly == Ethnocentric_Rg_glyphs.end())
			gly = Ethnocentric_Rg_glyphs.begin();

		rt.x+=gly->second.width;
		rt.y=max(gly->second.height,rt.y);
	}

	return rt;
}
