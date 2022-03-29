#ifndef __REGION_TABLE_HH__
#define __REGION_TABLE_HH__

#include "common.h"
#include "cache.h"
//�޷�����
using table_size_t = uint64_t;
constexpr uint64_t CACHELINE_END = (CACHELINE - 1);
constexpr uint64_t BOARD_BYTES = (CACHELINE - sizeof(uint64_t));

//���ʹ�ú����������ȸ�ֵ
#define CEIL_DIV(__x, __y) (((__x) + (__y) - 1) / (__y))



//������������״̬λ������
//��Ĵ���ģʽ
struct __RegisterTool {
	uint8_t cacheline[CACHELINE];
	status get_value(table_size_t idx_K_bits, uint64_t mask, uint64_t* value);
	status set_value(table_size_t idx_K_bits, uint64_t mask, uint64_t value, bool write_back = false);
};
//�ڲ������࣬�̶�ΪCACHELINE�ֽ�
struct __AlignedCacheline {
	uint8_t cacheline[CACHELINE];
	status get_value(table_size_t idx_K_bits, uint64_t mask, uint64_t* value);
	status set_value(table_size_t idx_K_bits, uint64_t mask, uint64_t value, bool write_back = false);
};

//������Ϊ��cacheline�����������
//�����unit_bits��û�б�Ҫ��������ͣ�����8bits,16bits,32bits,64bits
//���������֧��(64 - 7)��unit_bits
//�����û��棬�Ͳ�Ҫ��һ�����⻺��ʵ���ˣ�������ӳ�����£�����Ϊ��ֱ�Ӽ����ض���ʱ�ӡ�
class AlignedRegionIdxTable {
public:
	AlignedRegionIdxTable(table_size_t idx_range, table_size_t unit_bits);

	//�ᷢ��������������ڷ���__cacheilne_range��Щ˽�б����ᴥ�����ʻ��濪��
	status get_value(table_size_t idx_K, uint64_t* value);

	//�ṩ����cacheline����
	status get_cachline(table_size_t cacheline_idx, cacheline_t buffer, uint32_t n_bytes = CACHELINE);

	//����д���ӿڣ�����fore_index����
	status write_cacheline(table_size_t cacheline_idx, cacheline_t buffer, uint32_t n_bytes = CACHELINE);

	status get_bytes(table_size_t bytes_idx, addr_t buffer, uint32_t n_bytes);

	status set_value(table_size_t idx_K, uint64_t value, bool write_back = false);

	~AlignedRegionIdxTable();
private:
	//��Щ˽�б�����ÿ�η�����Ҫ�ӻ����ӳ٣�������Ϊ�����٣������Ƿ���Թ̶��浽ĳЩ�ط���
	addr_t __base;
	table_size_t __idx_range;
	table_size_t __unit_bits;
	table_size_t __cacheline_range;
	uint64_t __mask;
};

#endif

