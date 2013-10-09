R"(<?xml version="1.0" encoding="UTF-8" ?>

<material>
	<shaderProgram>
		<inputs>
			<input><type>mat4</type><name>modelViewProjectionMat</name><value></value><instanced>%instanced%</instanced></input>
			<input><type>mat3</type><name>normalMat</name><value></value><instanced>%instanced%</instanced></input>

			<input><type>vec2</type><name>specular</name><value>1.0 90.0</value></input>
			<input><type>float</type><name>blurring</name><value>0.0</value><const>1</const></input>
			<input><type>sampler2D</type><name>diffuseMap</name><value>%diffuseMap%</value></input>
			<input><type>sampler2D</type><name>normalMap</name><value>%normalMap%</value></input>
		</inputs>

		<shader>
			<type>vertex</type>
			<includes>
				<include>shaders/MsCommonVert.glsl</include>
			</includes>

			<operations>
				<operation>
					<id>1</id>
					<returnType>void</returnType>
					<function>writePositionNormalTangentTexCoord</function>
					<arguments><argument>modelViewProjectionMat</argument><argument>normalMat</argument></arguments>
				</operation>
			</operations>
		</shader>

		<shader>
			<type>fragment</type>

			<includes>
				<include>shaders/MsCommonFrag.glsl</include>
			</includes>
			
			<operations>
				<operation>
					<id>0</id>
					<returnType>vec3</returnType>
					<function>getNormal</function>
				</operation>
				<operation>
					<id>1</id>
					<returnType>vec4</returnType>
					<function>getTangent</function>
				</operation>
				<operation>
					<id>2</id>
					<returnType>vec2</returnType>
					<function>getTextureCoord</function>
				</operation>

				<operation>
					<id>10</id>
					<returnType>vec3</returnType>
					<function>readRgbFromTexture</function>
					<arguments>
						<argument>diffuseMap</argument>
						<argument>out2</argument>
					</arguments>
				</operation>
				<operation>
					<id>20</id>
					<returnType>vec3</returnType>
					<function>getNormalFromTexture</function>
					<arguments>
						<argument>out0</argument>
						<argument>out1</argument>
						<argument>normalMap</argument>
						<argument>out2</argument>
					</arguments>
				</operation>
				<operation>
					<id>30</id>
					<returnType>void</returnType>
					<function>writeFais</function>
					<arguments>
						<argument>out10</argument>
						<argument>out20</argument>
						<argument>specular</argument>
						<argument>blurring</argument>
					</arguments>
				</operation>
			</operations>
		</shader>
	</shaderProgram>
</material>)"

