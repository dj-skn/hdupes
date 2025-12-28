/* hdupes double-traversal prevention tree
 * See hdupes.c for license information */

#ifndef HDUPES_TRAVCHECK_H
#define HDUPES_TRAVCHECK_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NO_TRAVCHECK

/* Tree to track each directory traversed */
struct travcheck {
  struct travcheck *left;
  struct travcheck *right;
  uintmax_t hash;
  hdupes_ino_t inode;
  dev_t device;
};

/* De-allocate the travcheck tree */
void travcheck_free(struct travcheck *cur);
int traverse_check(const dev_t device, const hdupes_ino_t inode);

#endif /* NO_TRAVCHECK */

#ifdef __cplusplus
}
#endif

#endif /* HDUPES_TRAVCHECK_H */
