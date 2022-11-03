#ifndef SHARED_MEMORY
#define SHARED_MEMORY
#include <memory>
#include <semaphore.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/io.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <thread>
#include <stdexcept>
#include <string.h>
#include <functional>
#include <assert.h>
#include <sstream>
#include <QDebug>

struct bad_shared_memory_access : std::runtime_error {
	bad_shared_memory_access(const char * error)
		: std::runtime_error(error)
	{ }
};

template<typename T, class = typename std::is_copy_assignable<T>::type>
struct shared_memory {
	using type = T;
	using storage_type = typename std::aligned_storage<sizeof (type),  alignof(type)>::type;
	struct mmap_d {
		sem_t event1;
		sem_t event2;
		unsigned char icount;
		type value;
	};

	shared_memory(const shared_memory& rhs) = delete;
	shared_memory(const shared_memory&& rhs) = delete;
	shared_memory& operator=(const shared_memory& rhs) = delete;

	shared_memory(int shmd, mmap_d * mapped)
		: fd(shmd), d(mapped)
	{ }

	~shared_memory()
	{
		if(d && fd)
		{
			if(sem_wait(&d->event1) == 0)
			{
				--(d->icount);
				if(d->icount == 0)
				{
					sem_close(&d->event1);
					shm_unlink(typeid(T).name());

					munmap(d, sizeof(mmap_d));
					close(fd);
				}
				else {
					sem_post(&d->event1);
				}
			}
		}
	}

	shared_memory(shared_memory&& rhs) {
		std::swap(fd, rhs.fd);
		std::swap(d, rhs.d);
	}



	shared_memory& operator=(shared_memory&& rhs) {
		if(this != &rhs){
			std::swap(fd, rhs.fd);
			std::swap(d, rhs.d);
		}

		return *this;
	}


	bool read(type& v)
	{
		if(!d) throw bad_shared_memory_access("shared_memory refered to nullptr");
		if(fd <= 0) throw bad_shared_memory_access("shared_memory refered to invalid descriptor");

		bool ok = false;
		if(sem_wait(&d->event1) == 0)
		{
			v = d->value;
			sem_post(&d->event1);
			ok = true;
		}

		return ok;
	}

	bool commit(const type& v)
	{
		if(!d) throw bad_shared_memory_access("shared_memory refered to nullptr");
		if(fd <= 0) throw bad_shared_memory_access("shared_memory refered to invalid descriptor");

		bool ok = false;
		if(sem_wait(&d->event1) == 0)
		{
			d->value = v;
			sem_post(&d->event1);
			ok = true;
		}

		return ok;
	}

private:
	int fd = 0;
	mmap_d * d = nullptr;
};


template<typename T>
shared_memory<T> make_shared_memory() {
	int shmd = 0;
	const char * memory_name = typeid(T).name();
	using mapping_t = typename shared_memory<T>::mmap_d;

	mapping_t * d = nullptr;

	if((shmd = shm_open(memory_name, O_CREAT | O_RDWR | O_EXCL, 0777)) == -1) {
		if(errno == EEXIST && (shmd = shm_open(memory_name, O_CREAT | O_RDWR, 0777)) != -1) {
			void * addr = mmap(0, sizeof(mapping_t), PROT_WRITE | PROT_READ, MAP_SHARED, shmd, 0);
			if(addr == MAP_FAILED) {
				throw bad_shared_memory_access(strerror(errno));
			}

			d = (mapping_t*) addr;
			sem_wait(&d->event1);
			++(d->icount);
			sem_post(&d->event1);
		}

		else
		throw bad_shared_memory_access(strerror(errno));
	}

	else
	{
		if(ftruncate(shmd, sizeof (mapping_t)) == -1) {
			throw bad_shared_memory_access(strerror(errno));
		}

		fchmod(shmd, S_IRWXU | S_IRWXG | S_IRWXO);

		void * addr = mmap(0, sizeof(mapping_t), PROT_WRITE | PROT_READ, MAP_SHARED, shmd, 0);
		if(addr == MAP_FAILED) {
			throw bad_shared_memory_access(strerror(errno));
		}

		d = (mapping_t*) addr;
		if(sem_init(&d->event1, 1, 1) != 0) {
			throw bad_shared_memory_access(strerror(errno));
		}

		d->icount = 1;
	}

	return shared_memory<T>(shmd, d);
}

#endif
