import yaml
import copy
import sys

def duplicate_listeners(input_file, output_file, num_copies):
    with open(input_file, 'r') as file:
        data = yaml.safe_load(file)
    
    if not data or 'resources' not in data:
        print("Invalid YAML structure")
        return
    
    # 查找第一个Listener配置
    base_listener = None
    for resource in data['resources']:
        if resource.get('@type') == 'type.googleapis.com/envoy.config.listener.v3.Listener':
            base_listener = resource
            break
    
    if not base_listener:
        print("No Listener found in resources")
        return
    
    # 获取基础端口和名称
    base_port = base_listener['address']['socket_address']['port_value']
    base_name = base_listener['name']
    base_host = base_listener['address']['socket_address']['address']  # 从配置中提取的基础主机
    
    new_resources = []
    
    # 复制并修改配置
    for i in range(num_copies):
        new_listener = copy.deepcopy(base_listener)
        
        # 更新名称
        new_listener['name'] = f"{base_name.rsplit('_', 1)[0]}_{i}"
        
        # 更新监听端口
        new_port = base_port + i
        new_listener['address']['socket_address']['port_value'] = new_port
        
        # 更新匹配器中的端口号
        for filter_chain in new_listener.get('filter_chains', []):
            for filter in filter_chain.get('filters', []):
                if filter.get('name') == 'envoy.filters.network.http_connection_manager':
                    for http_filter in filter['typed_config']['http_filters']:
                        if http_filter.get('name') == 'envoy.extensions.common.matching.v3.ExtensionWithMatcher':
                            matchers = http_filter['typed_config']['xds_matcher']['matcher_list']['matchers']
                            for matcher in matchers:
                                predicates = matcher['predicate']['and_matcher']['predicate']
                                for pred in predicates:
                                    if 'single_predicate' in pred:
                                        sp = pred['single_predicate']
                                        input_cfg = sp['input']['typed_config']
                                        if input_cfg.get('header_name') == 'host':
                                            if 'value_match' in sp and 'exact' in sp['value_match']:
                                                current_host_port = sp['value_match']['exact']
                                                # 只替换匹配基础端口的配置
                                                if current_host_port.endswith(f":{base_port}"):
                                                    sp['value_match']['exact'] = f"{base_host}:{new_port}"
        
        new_resources.append(new_listener)
    
    # 替换原始资源
    data['resources'] = new_resources
    
    # 保存到新文件
    with open(output_file, 'w') as file:
        yaml.dump(data, file, sort_keys=False)
    print(f"Successfully created {num_copies} listeners in {output_file}")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python increment_ports.py <file_path>")
        sys.exit(1)
    
    file_path = sys.argv[1]
    output_file = "resources_listener_modified.yaml"
    
    try:
        num_copies = int(input("请输入要创建的Listener数量: "))
        if num_copies <= 0:
            print("数量必须是正整数")
        else:
            duplicate_listeners(file_path, output_file, num_copies)
    except ValueError:
        print("请输入有效的数字")