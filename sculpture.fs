#version 330 core
struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
};
struct DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};
struct PointLight {
    vec3 position;
    float constant;
    float linear;
    float quadratic;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

uniform Material material;
uniform DirLight dirLight;
uniform PointLight pointLights[4];
uniform vec3 uViewPos;

in VS_OUT{
    vec3 FragPos;
    vec3 Normal;
} fs_in;

out vec4 FragColor;

vec3 calcDir(DirLight L, vec3 N, vec3 V){
    vec3 Ldir = normalize(-L.direction);
    float diff = max(dot(N, Ldir), 0.0);
    vec3 R = reflect(-Ldir, N);
    float spec = pow(max(dot(V, R),0.0), material.shininess);
    return L.ambient*material.ambient + L.diffuse*diff*material.diffuse + L.specular*spec*material.specular;
}
vec3 calcPoint(PointLight L, vec3 N, vec3 V){
    vec3 Ldir = normalize(L.position - fs_in.FragPos);
    float diff = max(dot(N, Ldir), 0.0);
    vec3 R = reflect(-Ldir, N);
    float spec = pow(max(dot(V, R),0.0), material.shininess);
    float d = length(L.position - fs_in.FragPos);
    float att = 1.0 / (L.constant + L.linear*d + L.quadratic*d*d);
    vec3 col = L.ambient*material.ambient + L.diffuse*diff*material.diffuse + L.specular*spec*material.specular;
    return col * att;
}

void main(){
    vec3 N = normalize(fs_in.Normal);
    vec3 V = normalize(uViewPos - fs_in.FragPos);

    vec3 color = calcDir(dirLight,N,V);
    for(int i=0;i<4;i++) color += calcPoint(pointLights[i],N,V);

    // subtle tint
    color *= vec3(0.95, 0.98, 1.00);
    FragColor = vec4(color,1.0);
}
