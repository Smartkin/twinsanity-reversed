
long UndefinedFunction_00101768(void)

{
  void *pvVar1;
  undefined8 uVar2;
  long lVar3;
  undefined4 *memory;
  ulong uVar4;
  undefined *puVar5;
  undefined4 *puVar6;
  int *piVar7;
  long lVar8;
  ulong uVar9;
  uint uVar10;
  long lVar11;
  uint uVar12;
  long lVar13;
  int iVar14;
  undefined4 uVar15;
  undefined4 in_stack_00000000;
  int in_stack_00000010;
  int in_stack_00000020;
  
  memory = (undefined4 *)MemoryAllocate(0x14);
  *memory = 0xffffff;
  memory[0x2] = 0x2ff618;
  memory[0x3] = memory[0x3] & 0x80000000;
  memory[0x4] = memory[0x4] & 0xffffffc0;
  memory[0x1] = 0x0;
  if (true) {
    return (long)(int)memory;
  }
  memory = (undefined4 *)MemoryAllocate(0x10);
  *memory = 0xffffff;
  memory[0x2] = 0x2ff818;
  memory[0x3] = memory[0x3] & 0xffffffc0;
  memory[0x1] = 0x0;
  if (true) {
    return (long)(int)memory;
  }
  memory = (undefined4 *)MemoryAllocate(0xc);
  lVar3 = (long)(int)memory;
  puVar5 = &DAT_002ff858;
  if (true) goto LAB_00106c0c;
  memory = (undefined4 *)MemoryAllocate(0xc);
  lVar3 = (long)(int)memory;
  puVar5 = &DAT_002ff898;
  if (true) goto LAB_00106c0c;
  memory = (undefined4 *)MemoryAllocate(0xc);
  lVar3 = (long)(int)memory;
  puVar5 = &DAT_002ff718;
  if (true) goto LAB_00106c0c;
  memory = (undefined4 *)MemoryAllocate(0xc);
  lVar3 = (long)(int)memory;
  puVar5 = &DAT_002ff6d8;
  if (true) goto LAB_00106c0c;
  memory = (undefined4 *)MemoryAllocate(0x10);
  lVar3 = (long)(int)memory;
  memory[0x2] = 0x2ff798;
  *memory = 0xffffff;
  piVar7 = memory + 0x3;
  memory[0x1] = 0x0;
  if (true) {
LAB_00106958:
    uVar2 = 0x0;
LAB_0010695c:
    FUN_00209900(piVar7,uVar2);
    if (true) {
      return lVar3;
    }
    memory = (undefined4 *)MemoryAllocate(0x10);
    lVar3 = (long)(int)memory;
    puVar5 = &DAT_002ef508;
    if (false) {
      memory = (undefined4 *)MemoryAllocate(0xc);
      lVar3 = (long)(int)memory;
      puVar5 = &DAT_002ef4c8;
      if (true) goto LAB_00106c0c;
      memory = (undefined4 *)MemoryAllocate(0x10);
      lVar3 = (long)(int)memory;
      puVar5 = &DAT_002ef208;
LAB_001069bc:
      *memory = 0xffffff;
      *(undefined **)(memory + 0x2) = puVar5;
      *(undefined *)(memory + 0x3) = 0xff;
      memory[0x1] = 0x0;
      if (true) {
        return lVar3;
      }
      memory = (undefined4 *)MemoryAllocate(0x10);
      lVar3 = (long)(int)memory;
      puVar5 = &DAT_002f1a30;
      if (false) {
        memory = (undefined4 *)MemoryAllocate(0x10);
        lVar3 = (long)(int)memory;
        puVar5 = &DAT_002f1a70;
      }
    }
LAB_00106a0c:
    *(undefined **)(memory + 0x2) = puVar5;
    *memory = 0xffffff;
    memory[0x1] = 0x0;
    memory[0x3] = 0x0;
    if (true) {
      return lVar3;
    }
    memory = (undefined4 *)MemoryAllocate(0x10);
    *memory = 0xffffff;
    memory[0x2] = 0x2f19b0;
    *(undefined *)((int)memory + 0xd) = 0xff;
    memory[0x1] = 0x0;
    *(undefined *)(memory + 0x3) = 0xff;
    if (true) {
      return (long)(int)memory;
    }
    memory = (undefined4 *)MemoryAllocate(0x28);
    *memory = 0xffffff;
    memory[0x2] = 0x2ee5a0;
    memory[0x9] = memory[0x9] & 0xfffffff0;
    memory[0x1] = 0x0;
    if (true) {
      return (long)(int)memory;
    }
    memory = (undefined4 *)MemoryAllocate(0x68);
    *memory = 0xffffff;
    memory[0x2] = 0x2ee478;
    memory[0x19] = memory[0x19] & 0xfffffc00;
    memory[0x1] = 0x0;
    if (true) {
      return (long)(int)memory;
    }
    memory = (undefined4 *)MemoryAllocate(0xc);
    lVar3 = (long)(int)memory;
    puVar5 = &DAT_002ee5e0;
    if (true) goto LAB_00106c0c;
    memory = (undefined4 *)MemoryAllocate(0x28);
    lVar3 = (long)(int)memory;
    *memory = 0xffffff;
    memory[0x2] = 0x2ee4f0;
    memory[0x1] = 0x0;
    FUN_002098d8(0x0,(long)(int)(memory + 0x3));
    FUN_002098d8(0x0,(long)(int)(memory + 0x4));
    FUN_002098d8(0x0,(long)(int)(memory + 0x5));
    FUN_002098d8(0x0,(long)(int)(memory + 0x6));
    FUN_002098d8(0x0,(long)(int)(memory + 0x7));
    FUN_002098d8(0x0,(long)(int)(memory + 0x8));
    uVar10 = 0x0;
    lVar8 = (long)(int)(memory + 0x9);
  }
  else {
    memory = (undefined4 *)MemoryAllocate(0x10);
    lVar3 = (long)(int)memory;
    memory[0x2] = 0x2ff758;
    *memory = 0xffffff;
    piVar7 = memory + 0x3;
    memory[0x1] = 0x0;
    if (true) goto LAB_00106958;
    memory = (undefined4 *)MemoryAllocate(0x10);
    lVar3 = (long)(int)memory;
    uVar10 = memory[0x3] & 0xffffffc0;
    *memory = 0xffffff;
    memory[0x2] = 0x2ff658;
    if (true) goto LAB_00102714;
    memory = (undefined4 *)MemoryAllocate(0x10);
    *memory = 0xffffff;
    memory[0x2] = 0x2ff518;
    *(undefined *)((int)memory + 0xd) = 0xff;
    *(undefined *)((int)memory + 0xe) = 0xf0;
    memory[0x1] = 0x0;
    *(undefined *)(memory + 0x3) = 0xff;
    if (true) {
      return (long)(int)memory;
    }
    memory = (undefined4 *)MemoryAllocate(0x80);
    memory[0x2] = 0x2fe358;
    *memory = 0xffffff;
    memory[0x1] = 0x0;
    FUN_00235060((long)(int)(memory + 0x4));
    if (true) {
      return (long)(int)memory;
    }
    memory = (undefined4 *)MemoryAllocate(0xc);
    lVar3 = (long)(int)memory;
    puVar5 = &DAT_002fe318;
    if (true) goto LAB_00106c0c;
    memory = (undefined4 *)MemoryAllocate(0x10);
    lVar3 = (long)(int)memory;
    *(undefined *)(memory + 0x3) = 0xff;
    *memory = 0xffffff;
    memory[0x2] = 0x2ee748;
    uVar10 = memory[0x3] & 0xfffffeff;
    if (true) goto LAB_001067d0;
    memory = (undefined4 *)MemoryAllocate(0xc);
    lVar3 = (long)(int)memory;
    puVar5 = &DAT_002ff4d8;
    if (true) goto LAB_00106c0c;
    memory = (undefined4 *)MemoryAllocate(0xc);
    lVar3 = (long)(int)memory;
    puVar5 = &DAT_002fe118;
    if (true) goto LAB_00106c0c;
    memory = (undefined4 *)MemoryAllocate(0xc);
    lVar3 = (long)(int)memory;
    puVar5 = &DAT_002fe0d8;
    if (true) goto LAB_00106c0c;
    memory = (undefined4 *)MemoryAllocate(0xc);
    lVar3 = (long)(int)memory;
    puVar5 = &DAT_002fe098;
    if (true) goto LAB_00106c0c;
    memory = (undefined4 *)MemoryAllocate(0x20);
    *(undefined *)((int)memory + 0xd) = 0xff;
    *(undefined *)(memory + 0x3) = 0xff;
    memory[0x2] = 0x2fe9d8;
    *memory = 0xffffff;
    memory[0x1] = 0x0;
    *(ulong *)(memory + 0x2) = *(ulong *)(memory + 0x2) & 0xfe00ffffffffffff;
    if (true) {
      return (long)(int)memory;
    }
    memory = (undefined4 *)MemoryAllocate(0x30);
    memory[0x2] = 0x2fe998;
    *(undefined *)(memory + 0x3) = 0xff;
    *(undefined *)((int)memory + 0xd) = 0xff;
    *(undefined *)(memory + 0x4) = 0xff;
    *memory = 0xffffff;
    memory[0x1] = 0x0;
    uVar4 = *(ulong *)(memory + 0x2) & 0xff80ffffffffffff;
    if (true) {
LAB_00103134:
      *(ulong *)(memory + 0x2) = uVar4 & 0xff7fffffffffffff;
      *(undefined *)((int)memory + 0xf) = 0xff;
      if (true) {
        return (long)(int)memory;
      }
      memory = (undefined4 *)MemoryAllocate(0x10);
      lVar3 = (long)(int)memory;
      puVar5 = &DAT_002feb98;
      if (false) {
        memory = (undefined4 *)MemoryAllocate(0x10);
        lVar3 = (long)(int)memory;
        puVar5 = &DAT_002feb58;
        if (false) {
          memory = (undefined4 *)MemoryAllocate(0x10);
          lVar3 = (long)(int)memory;
          puVar5 = &DAT_002feb18;
          if (false) {
            memory = (undefined4 *)MemoryAllocate(0x10);
            lVar3 = (long)(int)memory;
            puVar5 = &DAT_002fead8;
            if (false) {
              memory = (undefined4 *)MemoryAllocate(0x10);
              lVar3 = (long)(int)memory;
              puVar5 = &DAT_002fea98;
              if (false) {
                memory = (undefined4 *)MemoryAllocate(0x10);
                lVar3 = (long)(int)memory;
                puVar5 = &DAT_002fea98;
                if (true) goto LAB_00106a0c;
                memory = (undefined4 *)MemoryAllocate(0x1c);
                lVar3 = (long)(int)memory;
                puVar5 = &DAT_002fe8d8;
                if (true) goto LAB_00106c0c;
                memory = (undefined4 *)MemoryAllocate(0x18);
                *(undefined *)(memory + 0x3) = 0xff;
                *memory = 0xffffff;
                memory[0x2] = 0x2fe898;
                memory[0x1] = 0x0;
                memory[0x3] = memory[0x3] & 0xffffe0ff | 0x200;
                memory[0x4] = 0x0;
                memory[0x5] = 0x0;
                if (true) {
                  return (long)(int)memory;
                }
                memory = (undefined4 *)MemoryAllocate(0x20);
                *memory = 0xffffff;
                memory[0x2] = 0x2fe858;
                *(undefined *)(memory + 0x3) = 0xff;
                memory[0x1] = 0x0;
                memory[0x4] = 0x0;
                memory[0x5] = 0x0;
                memory[0x6] = 0x0;
                memory[0x7] = 0x0;
                if (true) {
                  return (long)(int)memory;
                }
                memory = (undefined4 *)MemoryAllocate(0x10);
                lVar3 = (long)(int)memory;
                uVar10 = memory[0x3] & 0xfffffffe;
                *memory = 0xffffff;
                memory[0x2] = 0x2fe818;
                if (true) goto LAB_00106430;
                memory = (undefined4 *)MemoryAllocate(0xc);
                lVar3 = (long)(int)memory;
                puVar5 = &DAT_002fe7d8;
                if (true) goto LAB_00106c0c;
                memory = (undefined4 *)MemoryAllocate(0x20);
                memory[0x2] = 0x2fe798;
                *memory = 0xffffff;
                memory[0x1] = 0x0;
                *(undefined *)(memory + 0x7) = 0x0;
                if (true) {
                  return (long)(int)memory;
                }
                memory = (undefined4 *)MemoryAllocate(0x10);
                puVar5 = &DAT_002fe758;
                if (true) {
LAB_00106420:
                  lVar3 = (long)(int)memory;
                  uVar10 = 0x1;
                  goto LAB_00106428;
                }
                memory = (undefined4 *)MemoryAllocate(0x10);
                lVar3 = (long)(int)memory;
                puVar5 = &DAT_002fe698;
                if (true) {
LAB_00105b98:
                  *(undefined **)(memory + 0x2) = puVar5;
                  *memory = 0xffffff;
                  memory[0x1] = 0x0;
                  *(undefined *)(memory + 0x3) = 0x0;
                  if (true) {
                    return lVar3;
                  }
                  pvVar1 = MemoryAllocate(0x40);
                  uVar2 = FUN_0011d4a0((long)(int)pvVar1,0x1);
                  if (true) {
                    return uVar2;
                  }
                  memory = (undefined4 *)MemoryAllocate(0xc);
                  lVar3 = (long)(int)memory;
                  puVar5 = &DAT_002fec58;
                  if (true) goto LAB_00106c0c;
                  pvVar1 = MemoryAllocate(0x40);
                  uVar2 = FUN_0011d4a0((long)(int)pvVar1,0x2);
                  if (true) {
                    return uVar2;
                  }
                  memory = (undefined4 *)MemoryAllocate(0x14);
                  *(undefined *)((int)memory + 0xe) = 0xff;
                  *(undefined *)(memory + 0x3) = 0xff;
                  *(undefined *)((int)memory + 0xd) = 0xff;
                  *memory = 0xffffff;
                  memory[0x2] = 0x2fecd8;
                  memory[0x1] = 0x0;
                  memory[0x3] = memory[0x3] | 0xf000000;
                  memory[0x4] = 0x0;
                  if (true) {
                    return (long)(int)memory;
                  }
                  memory = (undefined4 *)MemoryAllocate(0x2c);
                  *memory = 0xffffff;
                  memory[0x2] = 0x2fec18;
                  memory[0xa] = memory[0xa] & 0xffffffc0;
                  memory[0x1] = 0x0;
                  memory[0x3] = 0x0;
                  memory[0x4] = 0x0;
                  memory[0x5] = 0x0;
                  memory[0x6] = 0x0;
                  memory[0x7] = 0x0;
                  memory[0x8] = 0x0;
                  memory[0x9] = 0x0;
                  if (true) {
                    return (long)(int)memory;
                  }
                  memory = (undefined4 *)MemoryAllocate(0x14);
                  memory[0x2] = 0x2febd8;
                  *memory = 0xffffff;
                  memory[0x1] = 0x0;
                  memory[0x3] = 0x0;
                  memory[0x4] = 0x0;
                  if (true) {
                    return (long)(int)memory;
                  }
                  memory = (undefined4 *)MemoryAllocate(0x10);
                  lVar3 = (long)(int)memory;
                  puVar5 = &DAT_002efc08;
                  if (false) {
                    pvVar1 = MemoryAllocate(0x10);
                    uVar2 = FUN_0011f658((long)(int)pvVar1);
                    if (true) {
                      return uVar2;
                    }
                    pvVar1 = MemoryAllocate(0x10);
                    uVar2 = FUN_0011f780((long)(int)pvVar1);
                    if (true) {
                      return uVar2;
                    }
                    memory = (undefined4 *)MemoryAllocate(0x10);
                    lVar3 = (long)(int)memory;
                    *(undefined2 *)(memory + 0x3) = 0x0;
                    memory[0x2] = 0x2eeec8;
                    *memory = 0xffffff;
                    uVar10 = memory[0x3] | 0x30000;
                    if (true) goto LAB_001067d0;
                    pvVar1 = MemoryAllocate(0x20);
                    uVar2 = FUN_0011f370((long)(int)pvVar1);
                    if (true) {
                      return uVar2;
                    }
                    memory = (undefined4 *)MemoryAllocate(0x10);
                    lVar3 = (long)(int)memory;
                    puVar5 = &DAT_002f1e30;
                    if (false) {
                      memory = (undefined4 *)MemoryAllocate(0x20);
                      lVar3 = (long)(int)memory;
                      puVar5 = &DAT_002f1df0;
                      if (true) goto LAB_00106c0c;
                      memory = (undefined4 *)MemoryAllocate(0x10);
                      lVar3 = (long)(int)memory;
                      puVar5 = &DAT_002eff48;
                      goto LAB_00105e08;
                    }
                  }
                  goto LAB_00106a0c;
                }
                memory = (undefined4 *)MemoryAllocate(0xc);
                lVar3 = (long)(int)memory;
                puVar5 = &DAT_002fe658;
                if (true) goto LAB_00106c0c;
                memory = (undefined4 *)MemoryAllocate(0x4c);
                *memory = 0xffffff;
                memory[0x2] = 0x2ee708;
                memory[0x3] = memory[0x3] & 0xfffffff0;
                memory[0x1] = 0x0;
                memory[0x12] = 0x3f800000;
                uVar10 = 0x0;
                memory[0x5] = 0x2;
                puVar6 = memory + 0x6;
                memory[0x4] = 0x3f800000;
                *(undefined *)(memory + 0xa) = 0x0;
                memory[0xb] = 0x0;
                memory[0xc] = 0x0;
                memory[0xd] = 0x0;
                memory[0xe] = 0x0;
                memory[0xf] = 0x0;
                memory[0x10] = 0x3f800000;
                memory[0x11] = 0x0;
                do {
                  *(undefined2 *)puVar6 = 0xffff;
                  uVar10 += 0x1;
                  puVar6 = (undefined4 *)((int)puVar6 + 0x2);
                } while (uVar10 < 0x8);
                memory[0xe] = 0x3e99999a;
                memory[0x5] = 0x15b010;
                if (true) {
                  return (long)(int)memory;
                }
                memory = (undefined4 *)MemoryAllocate(0xc);
                lVar3 = (long)(int)memory;
                puVar5 = &DAT_002fe018;
                if (true) goto LAB_00106c0c;
                memory = (undefined4 *)MemoryAllocate(0x14);
                lVar3 = (long)(int)memory;
                puVar5 = &DAT_002fdfd8;
                if (false) {
                  memory = (undefined4 *)MemoryAllocate(0x14);
                  lVar3 = (long)(int)memory;
                  puVar5 = &DAT_002fdf98;
                  if (false) {
                    memory = (undefined4 *)MemoryAllocate(0x14);
                    lVar3 = (long)(int)memory;
                    puVar5 = &DAT_002fdf58;
                  }
                }
                *(undefined **)(memory + 0x2) = puVar5;
                *memory = 0xffffff;
                memory[0x1] = 0x0;
                memory[0x3] = 0x0;
                memory[0x4] = 0x0;
                if (true) {
                  return lVar3;
                }
                memory = (undefined4 *)MemoryAllocate(0x10);
                lVar3 = (long)(int)memory;
                puVar5 = &DAT_002fdf18;
                if (true) {
LAB_00103d98:
                  *(undefined **)(memory + 0x2) = puVar5;
                  *memory = 0xffffff;
                  memory[0x1] = 0x0;
                  memory[0x3] = 0x0;
                  if (true) {
                    return lVar3;
                  }
                  memory = (undefined4 *)MemoryAllocate(0x10);
                  lVar3 = (long)(int)memory;
                  uVar15 = 0x3f800000;
                  puVar5 = &DAT_003020f8;
                  if (true) goto LAB_00105a68;
                  memory = (undefined4 *)MemoryAllocate(0x94);
                  lVar3 = (long)(int)memory;
                  memory[0x2] = 0x301db8;
                  *memory = 0xffffff;
                  lVar8 = (long)(int)(memory + 0x3);
                  memory[0x1] = 0x0;
                  lVar11 = 0x1;
                  lVar13 = 0x0;
                  if (false) {
                    memory = (undefined4 *)MemoryAllocate(0x1c);
                    memory[0x2] = 0x2eed88;
                    *memory = 0xffffff;
                    memory[0x1] = 0x0;
                    memory[0x3] = 0x0;
                    memory[0x4] = 0x0;
                    *(undefined2 *)(memory + 0x5) = 0x0;
                    memory[0x6] = 0x0;
                    if (true) {
                      return (long)(int)memory;
                    }
                    memory = (undefined4 *)MemoryAllocate(0x10);
                    lVar3 = (long)(int)memory;
                    uVar10 = memory[0x3] & 0xfffffffe;
                    *memory = 0xffffff;
                    memory[0x2] = 0x2eed48;
                    if (true) goto LAB_00106430;
                    memory = (undefined4 *)MemoryAllocate(0x10);
                    lVar3 = (long)(int)memory;
                    uVar10 = memory[0x3] & 0xfffffffe;
                    *memory = 0xffffff;
                    memory[0x2] = 0x2eed08;
                    if (true) goto LAB_00106430;
                    memory = (undefined4 *)MemoryAllocate(0xc);
                    lVar3 = (long)(int)memory;
                    puVar5 = &DAT_00301d78;
                    if (true) goto LAB_00106c0c;
                    memory = (undefined4 *)MemoryAllocate(0x98);
                    lVar3 = (long)(int)memory;
                    memory[0x2] = 0x301c38;
                    *memory = 0xffffff;
                    lVar8 = (long)(int)(memory + 0x4);
                    memory[0x1] = 0x0;
                    lVar11 = 0x0;
                    memory[0x3] = 0x0;
                    lVar13 = 0x0;
                    if (true) goto LAB_0010682c;
                    memory = (undefined4 *)MemoryAllocate(0xc);
                    lVar3 = (long)(int)memory;
                    puVar5 = &DAT_00301bf8;
                    if (true) goto LAB_00106c0c;
                    memory = (undefined4 *)MemoryAllocate(0xc);
                    lVar3 = (long)(int)memory;
                    puVar5 = &DAT_00301bb8;
                    if (true) goto LAB_00106c0c;
                    memory = (undefined4 *)MemoryAllocate(0xc);
                    lVar3 = (long)(int)memory;
                    puVar5 = &DAT_00301b78;
                    if (true) goto LAB_00106c0c;
                    memory = (undefined4 *)MemoryAllocate(0x10);
                    lVar3 = (long)(int)memory;
                    *(undefined2 *)(memory + 0x3) = 0x0;
                    memory[0x2] = 0x302078;
                    *memory = 0xffffff;
                    uVar10 = memory[0x3] | 0x1ff0000;
                    if (false) {
                      memory = (undefined4 *)MemoryAllocate(0x1c);
                      memory[0x2] = 0x301d38;
                      *memory = 0xffffff;
                      memory[0x1] = 0x0;
                      memory[0x3] = 0x0;
                      memory[0x4] = 0x0;
                      memory[0x5] = 0x0;
                      memory[0x6] = 0x0;
                      if (true) {
                        return (long)(int)memory;
                      }
                      memory = (undefined4 *)MemoryAllocate(0xc);
                      lVar3 = (long)(int)memory;
                      puVar5 = &DAT_00301cf8;
                      if (true) goto LAB_00106c0c;
                      pvVar1 = MemoryAllocate(0x10);
                      lVar3 = (long)(int)pvVar1;
                      uVar12 = 0xfffffcff;
                      uVar10 = *(uint *)((int)pvVar1 + 0xc) & 0xffffff00;
                      puVar5 = &DAT_002eecc8;
                      if (true) goto LAB_00105904;
                      memory = (undefined4 *)MemoryAllocate(0x30);
                      *memory = 0xffffff;
                      memory[0x2] = 0x301b38;
                      memory[0x1] = 0x0;
                      memory[0x4] = 0x0;
                      memory[0x7] = 0x3f800000;
                      memory[0x5] = 0x0;
                      memory[0x6] = 0x0;
                      memory[0x8] = 0x3f800000;
                      memory[0x9] = 0x0;
                      *(undefined *)(memory + 0xa) = 0x0;
                      if (true) {
                        return (long)(int)memory;
                      }
                      memory = (undefined4 *)MemoryAllocate(0x30);
                      lVar3 = (long)(int)memory;
                      memory[0x2] = 0x301ab8;
                      uVar9 = 0xff7fffffffffffff;
                      *memory = 0xffffff;
                      puVar6 = memory + 0x4;
                      memory[0x1] = 0x0;
                      uVar4 = *(ulong *)(memory + 0x2) & 0xff9e07f8ffffffff | 0x1e07f800000000;
                      if (true) {
LAB_00104a64:
                        iVar14 = (int)lVar3;
                        *(undefined4 *)(iVar14 + 0x10) = 0x0;
                        *(ulong *)(iVar14 + 0x8) = uVar4 & uVar9;
                        puVar6[0x1] = 0x0;
                        puVar6[0x3] = 0x3f800000;
                        puVar6[0x2] = 0x0;
                        FUN_0011bf10((float)puVar6[0x1],(int *)register0x000001d0,0x0);
                        FUN_0011bf10(0.00000000,&stack0x00000010,0x0);
                        FUN_0011bf10(0.00000000,&stack0x00000020,0x0);
                        *(undefined4 *)(iVar14 + 0x20) = in_stack_00000000;
                        *(int *)(iVar14 + 0x24) = in_stack_00000010;
                        *(int *)(iVar14 + 0x28) = in_stack_00000020;
                        if (true) {
                          return lVar3;
                        }
                        memory = (undefined4 *)MemoryAllocate(0x10);
                        *(undefined *)(memory + 0x3) = 0xff;
                        *memory = 0xffffff;
                        memory[0x2] = 0x2fef18;
                        memory[0x1] = 0x0;
                        memory[0x3] = memory[0x3] & 0xffffc0ff | 0x600;
                        if (true) {
                          return (long)(int)memory;
                        }
                        memory = (undefined4 *)MemoryAllocate(0x10);
                        lVar3 = (long)(int)memory;
                        *(undefined *)(memory + 0x3) = 0xff;
                        *memory = 0xffffff;
                        memory[0x2] = 0x2feed8;
                        uVar10 = memory[0x3] & 0xfffff8ff | 0x300;
                        goto LAB_00104b78;
                      }
                      memory = (undefined4 *)MemoryAllocate(0x50);
                      *(ulong *)(memory + 0xc) =
                           *(ulong *)(memory + 0xc) & 0xffffffffff0ffff0 | 0xffff2;
                      *memory = 0xffffff;
                      memory[0x2] = 0x301a78;
                      *(undefined *)((int)memory + 0x35) = 0xff;
                      memory[0x1] = 0x0;
                      *(undefined *)(memory + 0xd) = 0xff;
                      if (true) {
                        return (long)(int)memory;
                      }
                      memory = (undefined4 *)MemoryAllocate(0xc);
                      lVar3 = (long)(int)memory;
                      puVar5 = &DAT_00301a38;
                      if (true) goto LAB_00106c0c;
                      memory = (undefined4 *)MemoryAllocate(0x30);
                      lVar3 = (long)(int)memory;
                      memory[0x2] = 0x3019f8;
                      *(undefined *)((int)memory + 0xe) = 0xff;
                      *(undefined2 *)(memory + 0x3) = 0xffff;
                      uVar9 = 0xdfffffffffffffff;
                      *memory = 0xffffff;
                      puVar6 = memory + 0x4;
                      memory[0x1] = 0x0;
                      uVar4 = *(ulong *)(memory + 0x2) & 0xe0ffffffffffffff;
                      if (true) goto LAB_00104a64;
                      memory = (undefined4 *)MemoryAllocate(0x30);
                      memory[0x2] = 0x301978;
                      *(undefined *)(memory + 0x3) = 0xff;
                      *memory = 0xffffff;
                      memory[0x1] = 0x0;
                      memory[0x4] = 0x0;
                      *(ulong *)(memory + 0x2) =
                           *(ulong *)(memory + 0x2) & 0xffff0ffffffffff | 0xffff00000000000;
                      memory[0x7] = 0x3f800000;
                      memory[0x5] = 0x0;
                      memory[0x6] = 0x0;
                      FUN_002098d8(0xbf800000,(long)(int)(memory + 0x8));
                      FUN_002098d8(0x3f800000,(long)(int)(memory + 0x9));
                      memory[0xa] = 0x0;
                      if (true) {
                        return (long)(int)memory;
                      }
                      memory = (undefined4 *)MemoryAllocate(0xc);
                      lVar3 = (long)(int)memory;
                      puVar5 = &DAT_003018b8;
                      if (true) goto LAB_00106c0c;
                      memory = (undefined4 *)MemoryAllocate(0x10);
                      lVar3 = (long)(int)memory;
                      *(undefined *)(memory + 0x3) = 0xff;
                      *memory = 0xffffff;
                      memory[0x2] = 0x2fd918;
                      uVar10 = memory[0x3] & 0xfffffeff;
                      if (true) goto LAB_001067d0;
                      memory = (undefined4 *)MemoryAllocate(0x40);
                      *memory = 0xffffff;
                      memory[0x2] = 0x3017f8;
                      memory[0x1] = 0x0;
                      memory[0x3] = 0x0;
                      memory[0x4] = 0x0;
                      memory[0x5] = 0x0;
                      memory[0x8] = 0x0;
                      memory[0xb] = 0x3f800000;
                      memory[0x9] = 0x0;
                      memory[0xa] = 0x0;
                      *(ulong *)(memory + 0xc) = *(ulong *)(memory + 0xc) & 0xfffffffffffffffc;
                      if (true) {
                        return (long)(int)memory;
                      }
                      memory = (undefined4 *)MemoryAllocate(0xc);
                      lVar3 = (long)(int)memory;
                      puVar5 = &DAT_003017b8;
                      if (true) goto LAB_00106c0c;
                      memory = (undefined4 *)MemoryAllocate(0x14);
                      *(undefined *)((int)memory + 0xe) = 0xff;
                      *(undefined2 *)(memory + 0x3) = 0xffff;
                      memory[0x2] = 0x2ff118;
                      *memory = 0xffffff;
                      memory[0x3] = memory[0x3] & 0xf8ffffff;
                      memory[0x1] = 0x0;
                      memory[0x4] = 0x0;
                      if (true) {
                        return (long)(int)memory;
                      }
                      memory = (undefined4 *)MemoryAllocate(0x18);
                      lVar3 = (long)(int)memory;
                      *(undefined2 *)(memory + 0x3) = 0xffff;
                      *(undefined *)((int)memory + 0xe) = 0xff;
                      *memory = 0xffffff;
                      memory[0x2] = 0x2ff0d8;
                      memory[0x3] = memory[0x3] & 0xfcffffff;
                      *(undefined2 *)(memory + 0x5) = 0xffff;
                      if (false) {
                        memory = (undefined4 *)MemoryAllocate(0x14);
                        lVar3 = (long)(int)memory;
                        *memory = 0xffffff;
                        memory[0x2] = 0x2ff098;
                        *(undefined *)(memory + 0x3) = 0xff;
                        if (false) {
                          memory = (undefined4 *)MemoryAllocate(0x18);
                          *memory = 0xffffff;
                          memory[0x2] = 0x2ff058;
                          *(undefined *)(memory + 0x3) = 0xff;
                          memory[0x4] = 0xff;
                          memory[0x1] = 0x0;
                          memory[0x5] = 0x0;
                          if (true) {
                            return (long)(int)memory;
                          }
                          memory = (undefined4 *)MemoryAllocate(0xc);
                          lVar3 = (long)(int)memory;
                          puVar5 = &DAT_002fe198;
                          if (true) goto LAB_00106c0c;
                          pvVar1 = MemoryAllocate(0x10);
                          lVar3 = (long)(int)pvVar1;
                          uVar10 = *(uint *)((int)pvVar1 + 0xc) & 0xfffffff8;
                          puVar5 = &DAT_00301af8;
                          if (false) {
                            memory = (undefined4 *)MemoryAllocate(0x10);
                            lVar3 = (long)(int)memory;
                            *(undefined *)(memory + 0x3) = 0xff;
                            uVar12 = 0xffff7fff;
                            *memory = 0xffffff;
                            memory[0x2] = 0x2fed18;
                            uVar10 = memory[0x3] & 0xffff80ff | 0x1000;
                            if (true) goto LAB_00104dc0;
                            memory = (undefined4 *)MemoryAllocate(0x18);
                            uVar10 = memory[0x5];
                            uVar12 = 0xffff1;
                            goto LAB_00104718;
                          }
                          goto LAB_001060a4;
                        }
                      }
                      goto LAB_00105e80;
                    }
                    goto LAB_001067d0;
                  }
                  goto LAB_0010682c;
                }
                memory = (undefined4 *)MemoryAllocate(0x10);
                lVar3 = (long)(int)memory;
                puVar5 = &DAT_002fded8;
                if (true) goto LAB_00103d98;
                memory = (undefined4 *)MemoryAllocate(0x10);
                lVar3 = (long)(int)memory;
                puVar5 = &DAT_002fde98;
                if (true) goto LAB_00106a0c;
                memory = (undefined4 *)MemoryAllocate(0x10);
                lVar3 = (long)(int)memory;
                puVar5 = &DAT_002fde58;
                if (true) goto LAB_00106a0c;
                memory = (undefined4 *)MemoryAllocate(0xc);
                lVar3 = (long)(int)memory;
                puVar5 = &DAT_002fde18;
                if (true) goto LAB_00106c0c;
                memory = (undefined4 *)MemoryAllocate(0xc);
                lVar3 = (long)(int)memory;
                puVar5 = &DAT_002fddd8;
                if (true) goto LAB_00106c0c;
                memory = (undefined4 *)MemoryAllocate(0x1c);
                *(undefined2 *)(memory + 0x4) = 0xff;
                memory[0x1] = 0x0;
                memory[0x2] = 0x301738;
                *memory = 0xffffff;
                *(undefined2 *)(memory + 0x3) = 0xffff;
                *(undefined *)((int)memory + 0xe) = 0xff;
                memory[0x4] = memory[0x4] & 0xe0ffff | 0xe00000;
                memory[0x5] = 0x0;
                memory[0x6] = 0x0;
                if (true) {
                  return (long)(int)memory;
                }
                memory = (undefined4 *)MemoryAllocate(0xc);
                lVar3 = (long)(int)memory;
                puVar5 = &DAT_00301778;
                if (true) goto LAB_00106c0c;
                memory = (undefined4 *)MemoryAllocate(0x40);
                memory[0x2] = 0x3016f8;
                *memory = 0xffffff;
                *(undefined2 *)(memory + 0xc) = 0xffff;
                *(ulong *)(memory + 0x2) =
                     *(ulong *)(memory + 0x2) & 0xffff0000ffffffff | 0x200000000000;
                *(undefined *)((int)memory + 0xf) = 0xff;
                memory[0x1] = 0x0;
                *(undefined *)((int)memory + 0xe) = 0xff;
                if (true) {
                  return (long)(int)memory;
                }
                memory = (undefined4 *)MemoryAllocate(0x10);
                *memory = 0xffffff;
                memory[0x2] = 0x3016b8;
                *(undefined2 *)(memory + 0x3) = 0xffff;
                *(undefined *)((int)memory + 0xe) = 0xff;
                memory[0x1] = 0x0;
                if (true) {
                  return (long)(int)memory;
                }
                memory = (undefined4 *)MemoryAllocate(0x10);
                puVar5 = &DAT_00301638;
                if (false) {
                  memory = (undefined4 *)MemoryAllocate(0x14);
                  lVar3 = (long)(int)memory;
                  uVar15 = 0x3f800000;
                  memory[0x2] = 0x301678;
                  *memory = 0xffffff;
                  memory[0x3] = 0x41200000;
                  if (true) goto LAB_001052a8;
                  memory = (undefined4 *)MemoryAllocate(0x14);
                  *(undefined *)(memory + 0x3) = 0xff;
                  *memory = 0xffffff;
                  memory[0x2] = 0x2fe058;
                  memory[0x3] = memory[0x3] & 0xfffff0ff;
                  memory[0x4] = 0x3f800000;
                  memory[0x1] = 0x0;
                  if (true) {
                    return (long)(int)memory;
                  }
                  pvVar1 = MemoryAllocate(0x10);
                  lVar3 = (long)(int)pvVar1;
                  uVar10 = *(uint *)((int)pvVar1 + 0xc) & 0xfffffff8;
                  puVar5 = &DAT_002fee58;
                  if (false) {
                    pvVar1 = MemoryAllocate(0x10);
                    lVar3 = (long)(int)pvVar1;
                    puVar5 = &DAT_002fee58;
                    uVar10 = *(uint *)((int)pvVar1 + 0xc) & 0xfffffff8 | 0x2;
                    if (true) goto LAB_001060a8;
                    pvVar1 = MemoryAllocate(0x10);
                    lVar3 = (long)(int)pvVar1;
                    uVar10 = *(uint *)((int)pvVar1 + 0xc) & 0xfffffff8;
                    puVar5 = &DAT_002fee18;
                    if (true) goto LAB_001060a4;
                    pvVar1 = MemoryAllocate(0x10);
                    lVar3 = (long)(int)pvVar1;
                    puVar5 = &DAT_002fee18;
                    uVar10 = *(uint *)((int)pvVar1 + 0xc) & 0xfffffff8 | 0x2;
                    if (true) goto LAB_001060a8;
                    memory = (undefined4 *)MemoryAllocate(0x10);
                    lVar3 = (long)(int)memory;
                    *(undefined2 *)(memory + 0x3) = 0xffff;
                    *memory = 0xffffff;
                    memory[0x2] = 0x3015f8;
                    uVar10 = memory[0x3] | 0x10000;
                    if (true) goto LAB_001067d0;
                    memory = (undefined4 *)MemoryAllocate(0xc);
                    lVar3 = (long)(int)memory;
                    puVar5 = &DAT_002fe518;
                    if (true) goto LAB_00106c0c;
                    memory = (undefined4 *)MemoryAllocate(0xc);
                    lVar3 = (long)(int)memory;
                    puVar5 = &DAT_002fe4d8;
                    if (true) goto LAB_00106c0c;
                    memory = (undefined4 *)MemoryAllocate(0xc);
                    lVar3 = (long)(int)memory;
                    puVar5 = &DAT_002fe498;
                    if (true) goto LAB_00106c0c;
                    memory = (undefined4 *)MemoryAllocate(0xc);
                    lVar3 = (long)(int)memory;
                    puVar5 = &DAT_002fe458;
                    if (true) goto LAB_00106c0c;
                    memory = (undefined4 *)MemoryAllocate(0x10);
                    lVar3 = (long)(int)memory;
                    puVar5 = &DAT_002fe418;
                    if (true) goto LAB_00105b98;
                    memory = (undefined4 *)MemoryAllocate(0xc);
                    lVar3 = (long)(int)memory;
                    puVar5 = &DAT_002fe558;
                    if (true) goto LAB_00106c0c;
                    pvVar1 = MemoryAllocate(0x50);
                    uVar2 = FUN_00252ff0((long)(int)pvVar1);
                    if (true) {
                      return uVar2;
                    }
                    pvVar1 = MemoryAllocate(0x10);
                    lVar3 = (long)(int)pvVar1;
                    uVar12 = 0xffffff3f;
                    uVar10 = *(uint *)((int)pvVar1 + 0xc) & 0xffffffc0;
                    puVar5 = &DAT_002eec88;
                    if (true) goto LAB_00105904;
                    memory = (undefined4 *)MemoryAllocate(0xc);
                    lVar3 = (long)(int)memory;
                    puVar5 = &DAT_00301fb8;
                    if (true) goto LAB_00106c0c;
                    memory = (undefined4 *)MemoryAllocate(0xc);
                    lVar3 = (long)(int)memory;
                    puVar5 = &DAT_00301f78;
                    if (true) goto LAB_00106c0c;
                    memory = (undefined4 *)MemoryAllocate(0x10);
                    lVar3 = (long)(int)memory;
                    uVar10 = 0x0;
                    *memory = 0xffffff;
                    lVar8 = (long)(int)(memory + 0x3);
                    memory[0x2] = 0x301f38;
                    memory[0x1] = 0x0;
                    if (false) {
                      memory = (undefined4 *)MemoryAllocate(0x14);
                      lVar3 = (long)(int)memory;
                      uVar15 = 0x3f800000;
                      uVar10 = memory[0x3] & 0xfffffffe;
                      memory[0x2] = 0x302138;
                      *memory = 0xffffff;
                      if (true) goto LAB_001052a4;
                      memory = (undefined4 *)MemoryAllocate(0xc);
                      lVar3 = (long)(int)memory;
                      puVar5 = &DAT_00301ff8;
                      if (true) goto LAB_00106c0c;
                      memory = (undefined4 *)MemoryAllocate(0x30);
                      memory[0x2] = 0x301ef8;
                      *memory = 0xffffff;
                      memory[0x1] = 0x0;
                      memory[0x4] = 0x0;
                      *(ulong *)(memory + 0x2) = *(ulong *)(memory + 0x2) & 0xffffffe0ffffffff;
                      memory[0x7] = 0x3f800000;
                      memory[0x5] = 0x0;
                      memory[0x6] = 0x0;
                      memory[0x8] = 0x0;
                      memory[0x9] = 0x0;
                      memory[0xa] = 0x0;
                      if (true) {
                        return (long)(int)memory;
                      }
                      memory = (undefined4 *)MemoryAllocate(0xc);
                      lVar3 = (long)(int)memory;
                      puVar5 = &DAT_00301eb8;
                      if (true) goto LAB_00106c0c;
                      pvVar1 = MemoryAllocate(0x50);
                      uVar2 = FUN_002532d8((long)(int)pvVar1);
                      if (true) {
                        return uVar2;
                      }
                      memory = (undefined4 *)MemoryAllocate(0x30);
                      *memory = 0xffffff;
                      memory[0x2] = 0x301e38;
                      memory[0x1] = 0x0;
                      memory[0x4] = 0x0;
                      memory[0x7] = 0x3f800000;
                      memory[0x5] = 0x0;
                      memory[0x6] = 0x0;
                      memory[0xa] = 0x0;
                      memory[0xb] = 0x0;
                      *(ulong *)(memory + 0x8) =
                           *(ulong *)(memory + 0x8) & 0xfffffffffe0ffff0 | 0xffff2;
                      memory[0x9] = 0x0;
                      if (true) {
                        return (long)(int)memory;
                      }
                      memory = (undefined4 *)MemoryAllocate(0x30);
                      memory[0x2] = 0x301df8;
                      *(undefined *)(memory + 0x3) = 0xff;
                      *memory = 0xffffff;
                      memory[0x1] = 0x0;
                      memory[0x4] = 0x0;
                      *(ulong *)(memory + 0x2) = *(ulong *)(memory + 0x2) & 0xffffe0ffffffffff;
                      memory[0x7] = 0x3f800000;
                      memory[0x5] = 0x0;
                      memory[0x6] = 0x0;
                      memory[0x8] = 0x0;
                      memory[0xb] = 0x3f800000;
                      memory[0x9] = 0x0;
                      memory[0xa] = 0x0;
                      if (true) {
                        return (long)(int)memory;
                      }
                      memory = (undefined4 *)MemoryAllocate(0xc);
                      lVar3 = (long)(int)memory;
                      puVar5 = &DAT_00301cb8;
                      if (true) goto LAB_00106c0c;
                      memory = (undefined4 *)MemoryAllocate(0xc);
                      lVar3 = (long)(int)memory;
                      puVar5 = &DAT_00301c78;
                      if (true) goto LAB_00106c0c;
                      memory = (undefined4 *)MemoryAllocate(0x10);
                      lVar3 = (long)(int)memory;
                      puVar5 = &DAT_003020b8;
                      goto LAB_00103d98;
                    }
                    goto LAB_00106b80;
                  }
                  goto LAB_001060a4;
                }
                goto LAB_00106870;
              }
            }
          }
        }
      }
      goto LAB_00106a0c;
    }
    memory = (undefined4 *)MemoryAllocate(0x20);
    lVar3 = (long)(int)memory;
    puVar5 = &DAT_002fe958;
    if (true) goto LAB_00106c0c;
    memory = (undefined4 *)MemoryAllocate(0x30);
    *memory = 0xffffff;
    memory[0x2] = 0x2fe918;
    *(undefined *)((int)memory + 0x11) = 0xff;
    memory[0x1] = 0x0;
    *(undefined *)(memory + 0x4) = 0xff;
    if (true) {
      return (long)(int)memory;
    }
    memory = (undefined4 *)MemoryAllocate(0xc);
    lVar3 = (long)(int)memory;
    puVar5 = &DAT_002fea18;
    if (true) goto LAB_00106c0c;
    memory = (undefined4 *)MemoryAllocate(0x10);
    lVar3 = (long)(int)memory;
    *(undefined *)((int)memory + 0xe) = 0xff;
    *(undefined *)(memory + 0x3) = 0xff;
    *(undefined *)((int)memory + 0xd) = 0xff;
    *memory = 0xffffff;
    uVar10 = memory[0x3] & 0xe8ffffff | 0x8000000;
    memory[0x2] = 0x2fe618;
    if (false) {
      memory = (undefined4 *)MemoryAllocate(0xc);
      lVar3 = (long)(int)memory;
      puVar5 = &DAT_002fe5d8;
      if (true) goto LAB_00106c0c;
      memory = (undefined4 *)MemoryAllocate(0xc);
      lVar3 = (long)(int)memory;
      puVar5 = &DAT_002fe598;
      if (true) goto LAB_00106c0c;
      memory = (undefined4 *)MemoryAllocate(0x10);
      *(undefined2 *)(memory + 0x3) = 0x0;
      memory[0x2] = 0x2ff8d8;
      *memory = 0xffffff;
      memory[0x3] = memory[0x3] & 0xfffeffff;
      memory[0x1] = 0x0;
      if (true) {
        return (long)(int)memory;
      }
      memory = (undefined4 *)MemoryAllocate(0x14);
      *(undefined2 *)(memory + 0x4) = 0x0;
      uVar10 = memory[0x4] & 0xfffcffff | 0x40000;
      *memory = 0xffffff;
      memory[0x2] = 0x2ff418;
      *(undefined *)(memory + 0x3) = 0xff;
      *(undefined2 *)((int)memory + 0xe) = 0xff;
      if (false) {
        memory = (undefined4 *)MemoryAllocate(0x10);
        lVar3 = (long)(int)memory;
        *(undefined *)(memory + 0x3) = 0xff;
        *memory = 0xffffff;
        memory[0x2] = 0x2ff3d8;
        uVar10 = memory[0x3] & 0xfffffcff;
        memory[0x1] = 0x0;
        if (true) goto LAB_00104dc8;
        memory = (undefined4 *)MemoryAllocate(0x10);
        lVar3 = (long)(int)memory;
        puVar5 = &DAT_002ff398;
        if (true) goto LAB_001069bc;
        memory = (undefined4 *)MemoryAllocate(0x10);
        lVar3 = (long)(int)memory;
        *(undefined *)(memory + 0x3) = 0xff;
        *memory = 0xffffff;
        memory[0x2] = 0x2ff358;
        uVar10 = memory[0x3] & 0xfffffeff;
        if (true) goto LAB_001067d0;
        memory = (undefined4 *)MemoryAllocate(0x18);
        *memory = 0xffffff;
        memory[0x2] = 0x2ff258;
        memory[0x5] = 0x3f800000;
        memory[0x1] = 0x0;
        memory[0x3] = 0x3f800000;
        memory[0x4] = 0x3f800000;
        if (true) {
          return (long)(int)memory;
        }
        memory = (undefined4 *)MemoryAllocate(0xc);
        lVar3 = (long)(int)memory;
        puVar5 = &DAT_002ff5d8;
        if (true) goto LAB_00106c0c;
        memory = (undefined4 *)MemoryAllocate(0xc);
        lVar3 = (long)(int)memory;
        puVar5 = &DAT_002ff598;
        if (true) goto LAB_00106c0c;
        memory = (undefined4 *)MemoryAllocate(0x10);
        lVar3 = (long)(int)memory;
        puVar5 = &DAT_002ff7d8;
        if (true) goto LAB_001069bc;
        memory = (undefined4 *)MemoryAllocate(0x10);
        lVar3 = (long)(int)memory;
        puVar5 = &DAT_002ff698;
        if (true) goto LAB_00105b98;
        pvVar1 = MemoryAllocate(0x30);
        uVar2 = FUN_00225540((long)(int)pvVar1,0x0);
        if (true) {
          return uVar2;
        }
        pvVar1 = MemoryAllocate(0x30);
        uVar2 = FUN_00225540((long)(int)pvVar1,0x1);
        if (true) {
          return uVar2;
        }
        memory = (undefined4 *)MemoryAllocate(0xc);
        lVar3 = (long)(int)memory;
        puVar5 = &DAT_002fd8d8;
        if (true) goto LAB_00106c0c;
        memory = (undefined4 *)MemoryAllocate(0x10);
        lVar3 = (long)(int)memory;
        *(undefined *)(memory + 0x3) = 0xff;
        uVar12 = 0xfffffbff;
        memory[0x2] = 0x2ff498;
        *memory = 0xffffff;
        uVar10 = memory[0x3] & 0xfffffcff;
        if (true) {
LAB_00104dc0:
          *(undefined4 *)((int)lVar3 + 0x4) = 0x0;
          uVar10 &= uVar12;
        }
        else {
          memory = (undefined4 *)MemoryAllocate(0x10);
          lVar3 = (long)(int)memory;
          *(undefined *)(memory + 0x3) = 0xff;
          *memory = 0xffffff;
          memory[0x2] = 0x2ff458;
          memory[0x1] = 0x0;
          uVar10 = memory[0x3] & 0xfffff0ff;
          if (false) {
            memory = (undefined4 *)MemoryAllocate(0x18);
            *memory = 0xffffff;
            memory[0x2] = 0x2fe718;
            memory[0x1] = 0x0;
            SetUndefinedID?((short *)(memory + 0x3));
            *(undefined *)(memory + 0x4) = 0xff;
            *(undefined2 *)(memory + 0x3) = 0xffff;
            *(undefined2 *)((int)memory + 0xe) = 0xffff;
            memory[0x5] = 0x0;
            memory[0x4] = memory[0x4] & 0xfffff0ff;
            if (true) {
              return (long)(int)memory;
            }
            memory = (undefined4 *)MemoryAllocate(0x10);
            lVar3 = (long)(int)memory;
            uVar15 = 0x3f800000;
            puVar5 = &DAT_002eee48;
            if (true) goto LAB_00105a68;
            memory = (undefined4 *)MemoryAllocate(0xc);
            lVar3 = (long)(int)memory;
            puVar5 = &DAT_002eee08;
            if (true) goto LAB_00106c0c;
            memory = (undefined4 *)MemoryAllocate(0x10);
            lVar3 = (long)(int)memory;
            *(undefined *)(memory + 0x3) = 0xff;
            *memory = 0xffffff;
            memory[0x2] = 0x2fef18;
            memory[0x1] = 0x0;
            uVar10 = memory[0x3] & 0xffffc0ff;
            if (true) goto LAB_00104dc8;
            memory = (undefined4 *)MemoryAllocate(0x10);
            lVar3 = (long)(int)memory;
            *(undefined *)(memory + 0x3) = 0xff;
            *memory = 0xffffff;
            memory[0x2] = 0x2feed8;
            uVar10 = memory[0x3] & 0xfffff8ff;
            if (true) goto LAB_001067d0;
            memory = (undefined4 *)MemoryAllocate(0x10);
            lVar3 = (long)(int)memory;
            *(undefined *)(memory + 0x3) = 0xff;
            *memory = 0xffffff;
            memory[0x2] = 0x2feed8;
            uVar10 = memory[0x3] & 0xfffff8ff | 0x100;
            if (true) {
LAB_00104b78:
              memory[0x1] = 0x0;
              memory[0x3] = uVar10;
              if (true) {
                return lVar3;
              }
              pvVar1 = MemoryAllocate(0x10);
              lVar3 = (long)(int)pvVar1;
              puVar5 = &DAT_002fee58;
              uVar10 = *(uint *)((int)pvVar1 + 0xc) & 0xfffffff8 | 0x3;
              if (true) goto LAB_001060a8;
              memory = (undefined4 *)MemoryAllocate(0x60);
              *memory = 0xffffff;
              memory[0x2] = 0x2ff018;
              memory[0x1] = 0x0;
              memory[0x4] = 0x0;
              memory[0x7] = 0x3f800000;
              memory[0x5] = 0x0;
              memory[0x6] = 0x0;
              *(undefined *)(memory + 0x8) = 0xff;
              *(undefined *)((int)memory + 0x21) = 0xff;
              memory[0xb] = 0x3f800000;
              *(undefined *)(memory + 0x13) = 0xff;
              memory[0xc] = 0x0;
              memory[0xd] = 0x0;
              memory[0xe] = 0x0;
              memory[0xf] = 0x0;
              memory[0x10] = 0x0;
              memory[0x11] = 0x0;
              memory[0x12] = 0x0;
              memory[0x14] = 0x0;
              *(ulong *)(memory + 0x8) = *(ulong *)(memory + 0x8) & 0xfffffffe0000ffff | 0x1800000;
              if (true) {
                return (long)(int)memory;
              }
              memory = (undefined4 *)MemoryAllocate(0x20);
              *memory = 0xffffff;
              memory[0x2] = 0x2fef58;
              memory[0x3] = memory[0x3] & 0xfffffff8 | 0x3;
            }
            else {
              memory = (undefined4 *)MemoryAllocate(0x10);
              lVar3 = (long)(int)memory;
              *(undefined *)(memory + 0x3) = 0xff;
              *memory = 0xffffff;
              memory[0x2] = 0x2feed8;
              uVar10 = memory[0x3] & 0xfffff8ff | 0x200;
              if (true) goto LAB_00104b78;
              memory = (undefined4 *)MemoryAllocate(0x20);
              memory[0x2] = 0x2fef58;
              *memory = 0xffffff;
              memory[0x3] = memory[0x3] & 0xfffffff8;
              if (false) {
                memory = (undefined4 *)MemoryAllocate(0xc);
                lVar3 = (long)(int)memory;
                puVar5 = &DAT_002fedd8;
                if (true) goto LAB_00106c0c;
                memory = (undefined4 *)MemoryAllocate(0xc);
                lVar3 = (long)(int)memory;
                puVar5 = &DAT_003018f8;
                if (true) goto LAB_00106c0c;
                memory = (undefined4 *)MemoryAllocate(0x20);
                *(undefined *)((int)memory + 0xd) = 0xff;
                *(undefined *)(memory + 0x3) = 0xff;
                memory[0x2] = 0x301878;
                *memory = 0xffffff;
                memory[0x1] = 0x0;
                *(ulong *)(memory + 0x2) = *(ulong *)(memory + 0x2) & 0xffc0ffffffffffff;
                if (true) {
                  return (long)(int)memory;
                }
                memory = (undefined4 *)MemoryAllocate(0x14);
                uVar10 = memory[0x4] & 0xffffffe4 | 0x24;
                memory[0x2] = 0x301838;
                *memory = 0xffffff;
                memory[0x3] = 0x3f800000;
                goto LAB_00102428;
              }
            }
            memory[0x7] = 0x3f800000;
            memory[0x1] = 0x0;
            memory[0x4] = 0x0;
            memory[0x5] = 0x3f800000;
            memory[0x6] = 0x3f800000;
            if (true) {
              return (long)(int)memory;
            }
            memory = (undefined4 *)MemoryAllocate(0xc);
            lVar3 = (long)(int)memory;
            puVar5 = &DAT_002fed98;
            if (true) goto LAB_00106c0c;
            memory = (undefined4 *)MemoryAllocate(0x10);
            lVar3 = (long)(int)memory;
            *(undefined *)(memory + 0x3) = 0xff;
            uVar12 = 0xffff7fff;
            *memory = 0xffffff;
            memory[0x2] = 0x2fed18;
            uVar10 = memory[0x3] & 0xffff80ff | 0x3000;
            goto LAB_00104dc0;
          }
        }
LAB_00104dc8:
        *(uint *)((int)lVar3 + 0xc) = uVar10;
        if (true) {
          return lVar3;
        }
        memory = (undefined4 *)MemoryAllocate(0x14);
        *memory = 0xffffff;
        memory[0x2] = 0x2f00e0;
        memory[0x1] = 0x0;
        SetUndefinedID?((short *)(memory + 0x3));
        SetUndefinedID?((short *)((int)memory + 0xe));
        memory[0x4] = memory[0x4] & 0xffffff00 | 0xa1;
        if (true) {
          return (long)(int)memory;
        }
        memory = (undefined4 *)MemoryAllocate(0x40);
        lVar3 = (long)(int)memory;
        *memory = 0xffffff;
        memory[0x2] = 0x2f0098;
        memory[0x1] = 0x0;
        memory[0x4] = 0x0;
        memory[0x7] = 0x3f800000;
        memory[0x5] = 0x0;
        memory[0x6] = 0x0;
        memory[0x9] = 0x0;
        FUN_00209900(memory + 0xa,0x1);
        FUN_002098d8(0x0,(long)(int)(memory + 0xb));
        memory[0x8] = 0x0;
        *(ulong *)(memory + 0x8) = *(ulong *)(memory + 0x8) | 0x1fe0;
        memory[0x2] = 0x2f0008;
        *(ulong *)(memory + 0xc) = *(ulong *)(memory + 0xc) & 0xfffffffffffffff0 | 0xf1;
        FUN_002098d8(0x0,(long)(int)(memory + 0xd));
        uVar10 = 0x0;
        lVar8 = (long)(int)(memory + 0xe);
        if (true) goto LAB_00106b80;
        memory = (undefined4 *)MemoryAllocate(0x18);
        memory[0x2] = 0x2effc8;
        *memory = 0xffffff;
        memory[0x1] = 0x0;
        memory[0x3] = 0x0;
        if (true) goto LAB_0010678c;
        memory = (undefined4 *)MemoryAllocate(0x40);
        lVar3 = (long)(int)memory;
        *memory = 0xffffff;
        memory[0x2] = 0x2f0098;
        memory[0x1] = 0x0;
        memory[0x4] = 0x0;
        memory[0x7] = 0x3f800000;
        memory[0x5] = 0x0;
        memory[0x6] = 0x0;
        memory[0x9] = 0x0;
        FUN_00209900(memory + 0xa,0x1);
        FUN_002098d8(0x0,(long)(int)(memory + 0xb));
        memory[0x8] = 0x0;
        memory[0xc] = 0x0;
        memory[0x2] = 0x2f0050;
        piVar7 = memory + 0xd;
        uVar2 = 0x1;
        *(ulong *)(memory + 0x8) = *(ulong *)(memory + 0x8) | 0x1fe0;
        if (false) {
          memory = (undefined4 *)MemoryAllocate(0x10);
          lVar3 = (long)(int)memory;
          puVar5 = &DAT_002efdc8;
          if (true) {
LAB_00105e08:
            *(undefined **)(memory + 0x2) = puVar5;
            *memory = 0xffffff;
            memory[0x1] = 0x0;
            *(undefined2 *)(memory + 0x3) = 0x0;
            *(undefined2 *)((int)memory + 0xe) = 0x0;
            if (true) {
              return lVar3;
            }
            memory = (undefined4 *)MemoryAllocate(0x10);
            lVar3 = (long)(int)memory;
            puVar5 = &DAT_002eff88;
            if (true) goto LAB_00106c0c;
            memory = (undefined4 *)MemoryAllocate(0x18);
            lVar3 = (long)(int)memory;
            *memory = 0xffffff;
            memory[0x2] = 0x2eec48;
            memory[0x3] = memory[0x3] & 0xfffffffc;
          }
          else {
            memory = (undefined4 *)MemoryAllocate(0xc);
            lVar3 = (long)(int)memory;
            puVar5 = &DAT_002ef788;
            if (true) goto LAB_00106c0c;
            memory = (undefined4 *)MemoryAllocate(0xc);
            lVar3 = (long)(int)memory;
            puVar5 = &DAT_002ef488;
            if (true) goto LAB_00106c0c;
            memory = (undefined4 *)MemoryAllocate(0xc);
            lVar3 = (long)(int)memory;
            puVar5 = &DAT_002ef448;
            if (true) goto LAB_00106c0c;
            memory = (undefined4 *)MemoryAllocate(0x18);
            *memory = 0xffffff;
            memory[0x2] = 0x2ef748;
            memory[0x5] = 0x6;
            memory[0x1] = 0x0;
            memory[0x3] = 0x0;
            memory[0x4] = 0x6;
            if (true) {
              return (long)(int)memory;
            }
            memory = (undefined4 *)MemoryAllocate(0x14);
            lVar3 = (long)(int)memory;
            memory[0x2] = 0x2ef708;
            piVar7 = memory + 0x4;
            *memory = 0xffffff;
            uVar2 = 0x0;
            memory[0x3] = 0xffffffff;
            memory[0x1] = 0x0;
            if (true) goto LAB_0010695c;
            memory = (undefined4 *)MemoryAllocate(0x14);
            lVar3 = (long)(int)memory;
            *memory = 0xffffff;
            memory[0x2] = 0x2ef6c8;
            memory[0x3] = 0xffffffff;
            if (false) {
              memory = (undefined4 *)MemoryAllocate(0xc);
              lVar3 = (long)(int)memory;
              puVar5 = &DAT_002ef388;
              if (true) goto LAB_00106c0c;
              memory = (undefined4 *)MemoryAllocate(0x24);
              *memory = 0xffffff;
              memory[0x2] = 0x2ef348;
              memory[0x3] = memory[0x3] & 0xfffffff0 | 0x2;
              memory[0x1] = 0x0;
              memory[0x4] = 0x0;
              memory[0x5] = 0x0;
              memory[0x6] = 0x0;
              memory[0x7] = 0x0;
              memory[0x8] = 0x0;
              if (true) {
                return (long)(int)memory;
              }
              memory = (undefined4 *)MemoryAllocate(0x28);
              *memory = 0xffffff;
              memory[0x2] = 0x2ef308;
              memory[0x3] = 0xffffffff;
              memory[0x1] = 0x0;
              memory[0x6] = 0x0;
              memory[0x7] = 0x0;
              memory[0x8] = 0x0;
              memory[0x9] = 0x0;
              if (true) {
                return (long)(int)memory;
              }
              memory = (undefined4 *)MemoryAllocate(0xc);
              lVar3 = (long)(int)memory;
              puVar5 = &DAT_002ef2c8;
              if (true) goto LAB_00106c0c;
              memory = (undefined4 *)MemoryAllocate(0x24);
              memory[0x2] = 0x2ef248;
              *memory = 0xffffff;
              memory[0x1] = 0x0;
              memory[0x5] = 0x0;
              memory[0x6] = 0x0;
              memory[0x7] = 0x0;
              memory[0x8] = 0x0;
              if (true) {
                return (long)(int)memory;
              }
              memory = (undefined4 *)MemoryAllocate(0x10);
              lVar3 = (long)(int)memory;
              uVar15 = 0x3f800000;
              puVar5 = &DAT_002ef688;
              if (false) {
                memory = (undefined4 *)MemoryAllocate(0x10);
                lVar3 = (long)(int)memory;
                uVar15 = 0x3f800000;
                puVar5 = &DAT_002ef648;
                if (false) {
                  memory = (undefined4 *)MemoryAllocate(0x10);
                  lVar3 = (long)(int)memory;
                  uVar15 = 0x3f800000;
                  puVar5 = &DAT_002ef408;
                  if (false) {
                    memory = (undefined4 *)MemoryAllocate(0x10);
                    lVar3 = (long)(int)memory;
                    uVar15 = 0x3f800000;
                    puVar5 = &DAT_002ef3c8;
                    if (false) {
                      memory = (undefined4 *)MemoryAllocate(0x14);
                      lVar3 = (long)(int)memory;
                      uVar15 = 0xbf800000;
                      uVar10 = 0xffffffff;
                      *memory = 0xffffff;
                      memory[0x2] = 0x2ff318;
                      goto LAB_001052a4;
                    }
                  }
                }
              }
              goto LAB_00105a68;
            }
          }
LAB_00105e80:
          memory[0x1] = 0x0;
          memory[0x4] = 0x0;
          if (true) {
            return lVar3;
          }
          memory = (undefined4 *)MemoryAllocate(0xc);
          lVar3 = (long)(int)memory;
          puVar5 = &DAT_002ee908;
          if (true) goto LAB_00106c0c;
          memory = (undefined4 *)MemoryAllocate(0xc);
          lVar3 = (long)(int)memory;
          puVar5 = &DAT_002eec08;
          if (true) goto LAB_00106c0c;
          memory = (undefined4 *)MemoryAllocate(0x54);
          memory[0x2] = 0x2eebc8;
          *memory = 0xffffff;
          memory[0x7] = 0x40a00000;
          memory[0x10] = 0x3f800000;
          memory[0x1] = 0x0;
          memory[0x3] = 0x0;
          memory[0x4] = 0x0;
          memory[0x5] = 0x0;
          memory[0x6] = 0x3f800000;
          memory[0x8] = 0x0;
          memory[0x9] = 0x3f800000;
          memory[0xa] = 0x0;
          memory[0xb] = 0x0;
          memory[0xc] = 0x3f800000;
          memory[0xd] = 0x3f800000;
          memory[0xe] = 0x3f800000;
          memory[0xf] = 0x3f800000;
          memory[0x11] = 0x0;
          memory[0x12] = 0x0;
          if (true) {
            return (long)(int)memory;
          }
          memory = (undefined4 *)MemoryAllocate(0xc);
          lVar3 = (long)(int)memory;
          puVar5 = &DAT_002eeb48;
          if (true) goto LAB_00106c0c;
          memory = (undefined4 *)MemoryAllocate(0x3c);
          memory[0x2] = 0x2eea88;
          *memory = 0xffffff;
          memory[0x3] = memory[0x3] & 0xff000000 | 0x3030;
          memory[0x1] = 0x0;
          memory[0x4] = 0x0;
          memory[0x5] = 0x0;
          memory[0x6] = 0x0;
          memory[0x7] = 0x0;
          memory[0x8] = 0x0;
          memory[0x9] = 0x0;
          memory[0xa] = 0x0;
          memory[0xb] = 0x0;
          memory[0xc] = 0x0;
          memory[0xd] = 0x0;
          memory[0xe] = 0x0;
          if (true) {
            return (long)(int)memory;
          }
          memory = (undefined4 *)MemoryAllocate(0x10);
          lVar3 = (long)(int)memory;
          uVar10 = memory[0x3] & 0xfffffff8;
          *memory = 0xffffff;
          memory[0x2] = 0x2eea48;
          if (false) {
            pvVar1 = MemoryAllocate(0x10);
            lVar3 = (long)(int)pvVar1;
            uVar10 = *(uint *)((int)pvVar1 + 0xc) & 0xfffffff8;
            puVar5 = &DAT_002eea48;
LAB_001060a4:
            uVar10 |= 0x1;
            goto LAB_001060a8;
          }
          goto LAB_00106430;
        }
        goto LAB_0010695c;
      }
LAB_00102428:
      memory[0x4] = uVar10;
      memory[0x1] = 0x0;
      if (true) {
        return (long)(int)memory;
      }
      memory = (undefined4 *)MemoryAllocate(0x10);
      lVar3 = (long)(int)memory;
      puVar5 = &DAT_002fee98;
      if (false) {
        memory = (undefined4 *)MemoryAllocate(0x10);
        lVar3 = (long)(int)memory;
        uVar10 = memory[0x3] & 0xfffffff8;
        *memory = 0xffffff;
        memory[0x2] = 0x2fee58;
        if (false) {
          memory = (undefined4 *)MemoryAllocate(0x10);
          lVar3 = (long)(int)memory;
          uVar10 = memory[0x3] & 0xfffffff8;
          *memory = 0xffffff;
          memory[0x2] = 0x2fee18;
          if (false) {
            memory = (undefined4 *)MemoryAllocate(0x60);
            *memory = 0xffffff;
            memory[0x2] = 0x2ff018;
            memory[0x1] = 0x0;
            memory[0x4] = 0x0;
            memory[0x7] = 0x3f800000;
            memory[0x5] = 0x0;
            memory[0x6] = 0x0;
            *(undefined *)(memory + 0x8) = 0xff;
            *(undefined *)((int)memory + 0x21) = 0xff;
            memory[0xb] = 0x3f800000;
            *(undefined *)(memory + 0x13) = 0xff;
            memory[0xc] = 0x0;
            memory[0xd] = 0x0;
            memory[0xe] = 0x0;
            memory[0xf] = 0x0;
            memory[0x10] = 0x0;
            memory[0x11] = 0x0;
            memory[0x12] = 0x0;
            memory[0x14] = 0x0;
            *(ulong *)(memory + 0x8) = *(ulong *)(memory + 0x8) & 0xfffffffe0000ffff;
            if (true) {
              return (long)(int)memory;
            }
            memory = (undefined4 *)MemoryAllocate(0x20);
            *memory = 0xffffff;
            memory[0x2] = 0x2fefd8;
            memory[0x1] = 0x0;
            memory[0x4] = 0x0;
            memory[0x7] = 0x3f800000;
            memory[0x5] = 0x0;
            memory[0x6] = 0x0;
            if (true) {
              return (long)(int)memory;
            }
            memory = (undefined4 *)MemoryAllocate(0x14);
            lVar3 = (long)(int)memory;
            *memory = 0xffffff;
            memory[0x2] = 0x2fef98;
            memory[0x3] = 0x3f800000;
            if (true) goto LAB_00106278;
            pvVar1 = MemoryAllocate(0x10);
            lVar3 = (long)(int)pvVar1;
            uVar12 = 0xfffffcff;
            uVar10 = *(uint *)((int)pvVar1 + 0xc) & 0xffffff00;
            puVar5 = &DAT_002ef0c8;
            if (false) {
              memory = (undefined4 *)MemoryAllocate(0x10);
              lVar3 = (long)(int)memory;
              *(undefined *)(memory + 0x3) = 0xff;
              memory[0x2] = 0x2fed18;
              *memory = 0xffffff;
              uVar10 = memory[0x3] & 0xffff00ff;
              goto LAB_00102714;
            }
LAB_00105904:
            memory = (undefined4 *)lVar3;
            *memory = 0xffffff;
            *(undefined **)(memory + 0x2) = puVar5;
            memory[0x3] = uVar10 & uVar12;
            memory[0x1] = 0x0;
            if (true) {
              return lVar3;
            }
            memory = (undefined4 *)MemoryAllocate(0x10);
            lVar3 = (long)(int)memory;
            puVar5 = &DAT_002efb08;
            uVar10 = 0x6;
            if (false) {
              memory = (undefined4 *)MemoryAllocate(0x10);
              lVar3 = (long)(int)memory;
              puVar5 = &DAT_002efac8;
              uVar10 = 0x6;
              if (false) {
                memory = (undefined4 *)MemoryAllocate(0xc);
                lVar3 = (long)(int)memory;
                puVar5 = &DAT_002efa88;
                if (true) goto LAB_00106c0c;
                memory = (undefined4 *)MemoryAllocate(0xc);
                lVar3 = (long)(int)memory;
                puVar5 = &DAT_002efa48;
                if (true) goto LAB_00106c0c;
                memory = (undefined4 *)MemoryAllocate(0xc);
                lVar3 = (long)(int)memory;
                puVar5 = &DAT_002ef988;
                if (true) goto LAB_00106c0c;
                memory = (undefined4 *)MemoryAllocate(0x10);
                memory[0x2] = 0x2ef948;
                *memory = 0xffffff;
                memory[0x3] = memory[0x3] | 0x1;
                memory[0x1] = 0x0;
                if (true) {
                  return (long)(int)memory;
                }
                memory = (undefined4 *)MemoryAllocate(0x14);
                *memory = 0xffffff;
                memory[0x2] = 0x2ef908;
                memory[0x3] = memory[0x3] & 0xffffffef | 0x1fef;
                memory[0x4] = 0x6;
                memory[0x1] = 0x0;
                if (true) {
                  return (long)(int)memory;
                }
                memory = (undefined4 *)MemoryAllocate(0x10);
                lVar3 = (long)(int)memory;
                uVar15 = 0x420c0000;
                puVar5 = &DAT_002ef8c8;
                goto LAB_00105a68;
              }
            }
            goto LAB_00106428;
          }
        }
        goto LAB_00106430;
      }
      goto LAB_001069bc;
    }
LAB_00102714:
    memory[0x3] = uVar10;
    memory[0x1] = 0x0;
    if (true) {
      return lVar3;
    }
    memory = (undefined4 *)MemoryAllocate(0x18);
    uVar10 = memory[0x5];
    uVar12 = 0xffff0;
    if (false) {
      pvVar1 = MemoryAllocate(0x24);
      uVar2 = FUN_00221af8((long)(int)pvVar1);
      if (true) {
        return uVar2;
      }
      pvVar1 = MemoryAllocate(0x14);
      uVar2 = FUN_00221bd0((long)(int)pvVar1);
      if (true) {
        return uVar2;
      }
      memory = (undefined4 *)MemoryAllocate(0x10);
      lVar3 = (long)(int)memory;
      uVar10 = memory[0x3] & 0xfffffff0;
      *memory = 0xffffff;
      memory[0x2] = 0x2ff158;
      if (true) goto LAB_00106430;
      memory = (undefined4 *)MemoryAllocate(0x1c);
      lVar3 = (long)(int)memory;
      *memory = 0xffffff;
      memory[0x2] = 0x2fdd98;
      memory[0x1] = 0x0;
      FUN_002098d8(0x0,(long)(int)(memory + 0x4));
      FUN_002098d8(0x0,(long)(int)(memory + 0x5));
      FUN_002098d8(0x0,(long)(int)(memory + 0x6));
      memory = memory + 0x3;
      if (false) {
        memory = (undefined4 *)MemoryAllocate(0x24);
        lVar3 = (long)(int)memory;
        *memory = 0xffffff;
        memory[0x2] = 0x2fdd58;
        memory[0x1] = 0x0;
        FUN_002098d8(0x0,(long)(int)(memory + 0x4));
        FUN_002098d8(0x0,(long)(int)(memory + 0x5));
        FUN_002098d8(0x0,(long)(int)(memory + 0x6));
        FUN_002098d8(0x0,(long)(int)(memory + 0x7));
        FUN_002098d8(0x0,(long)(int)(memory + 0x8));
        memory = memory + 0x3;
        if (false) {
          memory = (undefined4 *)MemoryAllocate(0x20);
          lVar3 = (long)(int)memory;
          *memory = 0xffffff;
          memory[0x2] = 0x2fdd18;
          memory[0x1] = 0x0;
          FUN_002098d8(0x0,(long)(int)(memory + 0x4));
          FUN_002098d8(0x0,(long)(int)(memory + 0x5));
          FUN_002098d8(0x0,(long)(int)(memory + 0x6));
          FUN_002098d8(0x0,(long)(int)(memory + 0x7));
          memory = memory + 0x3;
          if (false) {
            memory = (undefined4 *)MemoryAllocate(0x24);
            lVar3 = (long)(int)memory;
            *memory = 0xffffff;
            memory[0x2] = 0x2fdcd8;
            memory[0x1] = 0x0;
            FUN_002098d8(0x0,(long)(int)(memory + 0x4));
            FUN_002098d8(0x0,(long)(int)(memory + 0x5));
            FUN_002098d8(0x0,(long)(int)(memory + 0x6));
            FUN_002098d8(0x0,(long)(int)(memory + 0x7));
            FUN_002098d8(0x0,(long)(int)(memory + 0x8));
            memory = memory + 0x3;
            if (false) {
              puVar6 = (undefined4 *)MemoryAllocate(0x10);
              puVar5 = &DAT_002fdc98;
              if (false) {
                puVar6 = (undefined4 *)MemoryAllocate(0x10);
                puVar5 = &DAT_002fdc58;
              }
              lVar3 = (long)(int)puVar6;
              *(undefined **)(puVar6 + 0x2) = puVar5;
              *puVar6 = 0xffffff;
              memory = puVar6 + 0x3;
              puVar6[0x1] = 0x0;
            }
          }
        }
      }
      FillMemory(memory,0x0,0x4);
      if (true) {
        return lVar3;
      }
      memory = (undefined4 *)MemoryAllocate(0x30);
      lVar3 = (long)(int)memory;
      puVar5 = &DAT_002fe3d8;
      if (true) goto LAB_00106c0c;
      memory = (undefined4 *)MemoryAllocate(0x30);
      *memory = 0xffffff;
      memory[0x2] = 0x2fe398;
      memory[0x1] = 0x0;
      SetUndefinedID?((short *)(memory + 0x3));
      memory[0x4] = 0x40a00000;
      *(ulong *)(memory + 0x2) = *(ulong *)(memory + 0x2) & 0xffffffffffff | 0x2000000000000000;
      memory[0x5] = 0x0;
      memory[0x6] = 0x0;
      memory[0x7] = 0x0;
      memory[0x8] = 0x0;
      memory[0xb] = 0x3f800000;
      memory[0x9] = 0x0;
      memory[0xa] = 0x0;
      if (true) {
        return (long)(int)memory;
      }
      memory = (undefined4 *)MemoryAllocate(0x48);
      *memory = 0xffffff;
      memory[0x2] = 0x2fdc18;
      memory[0x3] = memory[0x3] & 0xff7800f0 | 0x7800f0;
      memory[0x1] = 0x0;
      FUN_002098d8(0x0,(long)(int)(memory + 0x8));
      memory[0xf] = 0x0;
      memory[0x9] = 0x3f000000;
      memory[0x10] = 0x0;
      memory[0x11] = 0x0;
      if (true) {
        return (long)(int)memory;
      }
      memory = (undefined4 *)MemoryAllocate(0x14);
      lVar3 = (long)(int)memory;
      *(undefined *)((int)memory + 0xd) = 0xff;
      *(undefined *)(memory + 0x3) = 0xff;
      uVar15 = 0xbf800000;
      memory[0x2] = 0x2fda58;
      *memory = 0xffffff;
      memory[0x3] = memory[0x3] & 0xfffeffff;
      if (false) {
        memory = (undefined4 *)MemoryAllocate(0x14);
        memory[0x2] = 0x2fdad8;
        *memory = 0xffffff;
        memory[0x4] = memory[0x4] | 0x1;
        memory[0x1] = 0x0;
        memory[0x3] = 0x0;
        if (true) {
          return (long)(int)memory;
        }
        memory = (undefined4 *)MemoryAllocate(0xc);
        lVar3 = (long)(int)memory;
        puVar5 = &DAT_002fda98;
        if (true) goto LAB_00106c0c;
        memory = (undefined4 *)MemoryAllocate(0xc);
        lVar3 = (long)(int)memory;
        puVar5 = &DAT_002fda18;
        if (true) goto LAB_00106c0c;
        memory = (undefined4 *)MemoryAllocate(0xc);
        lVar3 = (long)(int)memory;
        puVar5 = &DAT_002fd9d8;
        if (true) goto LAB_00106c0c;
        memory = (undefined4 *)MemoryAllocate(0x18);
        lVar3 = (long)(int)memory;
        uVar15 = 0x3f800000;
        uVar10 = memory[0x3] & 0xfffffffe;
        memory[0x2] = 0x2fd998;
        *memory = 0xffffff;
        if (true) {
LAB_001052a4:
          memory[0x3] = uVar10;
          goto LAB_001052a8;
        }
        memory = (undefined4 *)MemoryAllocate(0x28);
        *(undefined *)(memory + 0x3) = 0xff;
        *memory = 0xffffff;
        memory[0x2] = 0x2fdb18;
        memory[0x3] = memory[0x3] & 0xfffffcff;
        memory[0x1] = 0x0;
        memory[0x6] = 0x3f800000;
        memory[0x4] = 0x0;
        memory[0x5] = 0x0;
        memory[0x9] = memory[0x9] & 0xfffffffe;
        if (true) {
          return (long)(int)memory;
        }
        memory = (undefined4 *)MemoryAllocate(0x20);
        *(undefined *)(memory + 0x4) = 0x0;
        *memory = 0xffffff;
        memory[0x2] = 0x2fdbd8;
        memory[0x5] = 0x3f000000;
        memory[0x4] = memory[0x4] & 0xffff8fff | 0x8f00;
        memory[0x6] = 0x40400000;
        memory[0x1] = 0x0;
        memory[0x3] = 0x0;
        memory[0x7] = 0x0;
        if (true) {
          return (long)(int)memory;
        }
        memory = (undefined4 *)MemoryAllocate(0x10);
        lVar3 = (long)(int)memory;
        uVar15 = 0x40400000;
        puVar5 = &DAT_002fdb98;
        if (false) {
          memory = (undefined4 *)MemoryAllocate(0x14);
          memory[0x2] = 0x2fdb58;
          *memory = 0xffffff;
          memory[0x3] = 0x40400000;
          memory[0x4] = memory[0x4] & 0xfffffff8;
          memory[0x1] = 0x0;
          if (true) {
            return (long)(int)memory;
          }
          memory = (undefined4 *)MemoryAllocate(0x94);
          lVar3 = (long)(int)memory;
          memory[0x2] = 0x2fe2d8;
          *memory = 0xffffff;
          lVar8 = (long)(int)(memory + 0x3);
          memory[0x1] = 0x0;
          lVar11 = 0x0;
          lVar13 = 0x0;
          if (false) {
            pvVar1 = MemoryAllocate(0x1c);
            lVar3 = (long)(int)pvVar1;
            uVar10 = *(uint *)((int)pvVar1 + 0x18) & 0xfffffffc;
            puVar5 = &DAT_002fe298;
            if (false) {
              pvVar1 = MemoryAllocate(0x1c);
              lVar3 = (long)(int)pvVar1;
              uVar10 = *(uint *)((int)pvVar1 + 0x18) & 0xfffffffc;
              puVar5 = &DAT_002fe258;
              if (false) {
                pvVar1 = MemoryAllocate(0x1c);
                lVar3 = (long)(int)pvVar1;
                uVar10 = *(uint *)((int)pvVar1 + 0x18) & 0xfffffffc;
                puVar5 = &DAT_002fe218;
              }
            }
            memory = (undefined4 *)lVar3;
            *memory = 0xffffff;
            *(undefined **)(memory + 0x2) = puVar5;
            memory[0x6] = uVar10 & 0xfffffffb;
            memory[0x1] = 0x0;
            memory[0x3] = 0x0;
            memory[0x4] = 0x0;
            memory[0x5] = 0x0;
            if (true) {
              return lVar3;
            }
            memory = (undefined4 *)MemoryAllocate(0xc);
            lVar3 = (long)(int)memory;
            puVar5 = &DAT_002fe1d8;
            if (true) goto LAB_00106c0c;
            pvVar1 = MemoryAllocate(0x10);
            uVar2 = FUN_00221170((long)(int)pvVar1);
            if (true) {
              return uVar2;
            }
            memory = (undefined4 *)MemoryAllocate(0x20);
            *(undefined *)((int)memory + 0xd) = 0xff;
            *(undefined *)(memory + 0x3) = 0xff;
            memory[0x2] = 0x2fe9d8;
            *memory = 0xffffff;
            memory[0x1] = 0x0;
            *(ulong *)(memory + 0x2) =
                 *(ulong *)(memory + 0x2) & 0xfe00ffffffffffff | 0x40000000000000;
            if (true) {
              return (long)(int)memory;
            }
            memory = (undefined4 *)MemoryAllocate(0x30);
            memory[0x2] = 0x2fe998;
            *(undefined *)(memory + 0x3) = 0xff;
            *(undefined *)((int)memory + 0xd) = 0xff;
            *(undefined *)(memory + 0x4) = 0xff;
            *memory = 0xffffff;
            memory[0x1] = 0x0;
            uVar4 = *(ulong *)(memory + 0x2) & 0xff80ffffffffffff | 0x40000000000000;
            goto LAB_00103134;
          }
          goto LAB_0010682c;
        }
LAB_00105a68:
        *memory = 0xffffff;
        *(undefined **)(memory + 0x2) = puVar5;
        memory[0x3] = uVar15;
        memory[0x1] = 0x0;
        if (true) {
          return lVar3;
        }
        pvVar1 = MemoryAllocate(0x18);
        uVar2 = FUN_001216d8((long)(int)pvVar1);
        if (true) {
          return uVar2;
        }
        memory = (undefined4 *)MemoryAllocate(0xc);
        lVar3 = (long)(int)memory;
        puVar5 = &DAT_002efb48;
        if (true) goto LAB_00106c0c;
        pvVar1 = MemoryAllocate(0x40);
        uVar2 = FUN_00121130((long)(int)pvVar1);
        if (true) {
          return uVar2;
        }
        memory = (undefined4 *)MemoryAllocate(0xc);
        lVar3 = (long)(int)memory;
        puVar5 = &DAT_002ef188;
        if (true) goto LAB_00106c0c;
        memory = (undefined4 *)MemoryAllocate(0xc);
        lVar3 = (long)(int)memory;
        puVar5 = &DAT_002fec98;
        if (true) goto LAB_00106c0c;
        memory = (undefined4 *)MemoryAllocate(0xc);
        lVar3 = (long)(int)memory;
        puVar5 = &DAT_002fed58;
        if (true) goto LAB_00106c0c;
        memory = (undefined4 *)MemoryAllocate(0x14);
        lVar3 = (long)(int)memory;
        *(undefined *)(memory + 0x3) = 0xff;
        *memory = 0xffffff;
        memory[0x2] = 0x2eefc8;
        memory[0x3] = memory[0x3] & 0xfffffeff;
        if (false) {
          pvVar1 = MemoryAllocate(0x40);
          uVar2 = FUN_0011d4a0((long)(int)pvVar1,0x0);
          if (true) {
            return uVar2;
          }
          memory = (undefined4 *)MemoryAllocate(0x10);
          lVar3 = (long)(int)memory;
          puVar5 = &DAT_002ef048;
          goto LAB_00105b98;
        }
        goto LAB_00106278;
      }
LAB_001052a8:
      memory[0x4] = uVar15;
      memory[0x1] = 0x0;
      if (true) {
        return lVar3;
      }
      memory = (undefined4 *)MemoryAllocate(0xc);
      lVar3 = (long)(int)memory;
      puVar5 = &DAT_002ff2d8;
      if (true) goto LAB_00106c0c;
      memory = (undefined4 *)MemoryAllocate(0xc);
      lVar3 = (long)(int)memory;
      puVar5 = &DAT_002ff298;
      if (true) goto LAB_00106c0c;
      memory = (undefined4 *)MemoryAllocate(0x14);
      *memory = 0xffffff;
      memory[0x2] = 0x2efd88;
      memory[0x4] = 0xffffffff;
      if (true) {
LAB_0010565c:
        memory[0x1] = 0x0;
        *(undefined2 *)(memory + 0x3) = 0x0;
        *(undefined2 *)((int)memory + 0xe) = 0x0;
        if (true) {
          return (long)(int)memory;
        }
        memory = (undefined4 *)MemoryAllocate(0xc);
        lVar3 = (long)(int)memory;
        puVar5 = &DAT_002f1eb0;
        if (true) goto LAB_00106c0c;
        memory = (undefined4 *)MemoryAllocate(0x3c);
        *memory = 0xffffff;
        memory[0x2] = 0x2f01e8;
        memory[0x1] = 0x0;
        FUN_002098d8(0x42480000,(long)(int)(memory + 0x3));
        FUN_002098d8(0x0,(long)(int)(memory + 0x4));
        FUN_002098d8(0x0,(long)(int)(memory + 0x5));
        FUN_002098d8(0x0,(long)(int)(memory + 0x6));
        FUN_002098d8(0x0,(long)(int)(memory + 0x7));
        FUN_002098d8(0x0,(long)(int)(memory + 0x8));
        FUN_002098d8(0x0,(long)(int)(memory + 0x9));
        memory[0xa] = memory[0xa] & 0xfffffff0;
        memory[0x2] = 0x2f01a0;
        FUN_002098d8(0x0,(long)(int)(memory + 0xb));
        FUN_002098d8(0x0,(long)(int)(memory + 0xc));
        FUN_002098d8(0x0,(long)(int)(memory + 0xd));
        memory[0xe] = 0xff;
        if (true) {
          return (long)(int)memory;
        }
        memory = (undefined4 *)MemoryAllocate(0x2c);
        *memory = 0xffffff;
        memory[0x2] = 0x2f01e8;
        memory[0x1] = 0x0;
        FUN_002098d8(0x42480000,(long)(int)(memory + 0x3));
        FUN_002098d8(0x0,(long)(int)(memory + 0x4));
        FUN_002098d8(0x0,(long)(int)(memory + 0x5));
        FUN_002098d8(0x0,(long)(int)(memory + 0x6));
        FUN_002098d8(0x0,(long)(int)(memory + 0x7));
        FUN_002098d8(0x0,(long)(int)(memory + 0x8));
        FUN_002098d8(0x0,(long)(int)(memory + 0x9));
        memory[0xa] = memory[0xa] & 0xfffffff0;
        if (true) {
          return (long)(int)memory;
        }
        memory = (undefined4 *)MemoryAllocate(0x10);
        lVar3 = (long)(int)memory;
        uVar10 = 0x3f800000;
        *memory = 0xffffff;
        lVar8 = (long)(int)(memory + 0x3);
        memory[0x2] = 0x2f0160;
        memory[0x1] = 0x0;
        if (false) {
          memory = (undefined4 *)MemoryAllocate(0x10);
          lVar3 = (long)(int)memory;
          puVar5 = &DAT_002f0120;
          if (false) {
            memory = (undefined4 *)MemoryAllocate(0xc);
            lVar3 = (long)(int)memory;
            puVar5 = &DAT_002efbc8;
            if (true) goto LAB_00106c0c;
            memory = (undefined4 *)MemoryAllocate(0xc);
            lVar3 = (long)(int)memory;
            puVar5 = &DAT_002efb88;
            if (true) goto LAB_00106c0c;
            memory = (undefined4 *)MemoryAllocate(0x10);
            lVar3 = (long)(int)memory;
            puVar5 = &DAT_002ef9c8;
            if (true) goto LAB_00106c0c;
            pvVar1 = MemoryAllocate(0x10);
            lVar3 = (long)(int)pvVar1;
            uVar12 = 0xfffffff7;
            uVar10 = *(uint *)((int)pvVar1 + 0xc) & 0xfffffff8;
            puVar5 = &DAT_002efa08;
            goto LAB_00105904;
          }
          goto LAB_00105e08;
        }
        goto LAB_00106b80;
      }
      memory = (undefined4 *)MemoryAllocate(0xc);
      lVar3 = (long)(int)memory;
      puVar5 = &DAT_002f1e70;
      if (true) goto LAB_00106c0c;
      memory = (undefined4 *)MemoryAllocate(0x54);
      memory[0x2] = 0x2ef008;
      *memory = 0xffffff;
      memory[0x1] = 0x0;
      FUN_0023ecb0((long)(int)(memory + 0x3));
      if (true) {
        return (long)(int)memory;
      }
      memory = (undefined4 *)MemoryAllocate(0x10);
      lVar3 = (long)(int)memory;
      uVar15 = 0x3f800000;
      puVar5 = &DAT_002ff218;
      if (true) goto LAB_00105a68;
      memory = (undefined4 *)MemoryAllocate(0x24);
      lVar3 = (long)(int)memory;
      *memory = 0xffffff;
      memory[0x2] = 0x2ee620;
      memory[0x5] = memory[0x5] & 0xfffffff8;
      memory[0x7] = 0x3f800000;
      *(undefined *)((int)memory + 0x22) = 0xff;
      memory[0x1] = 0x0;
      *(undefined *)(memory + 0x3) = 0xff;
      memory[0x4] = 0x0;
      if (false) {
        pvVar1 = MemoryAllocate(0x24);
        lVar3 = (long)(int)pvVar1;
        uVar10 = *(uint *)((int)pvVar1 + 0x14) & 0xfffffff8;
        uVar15 = 0x2;
        if (false) {
          pvVar1 = MemoryAllocate(0x24);
          lVar3 = (long)(int)pvVar1;
          uVar10 = *(uint *)((int)pvVar1 + 0x14) & 0xfffffff8;
          uVar15 = 0x1;
          if (false) {
            pvVar1 = MemoryAllocate(0x24);
            lVar3 = (long)(int)pvVar1;
            uVar10 = *(uint *)((int)pvVar1 + 0x14) & 0xfffffff8;
            uVar15 = 0x3;
            if (false) {
              pvVar1 = MemoryAllocate(0x24);
              lVar3 = (long)(int)pvVar1;
              uVar10 = *(uint *)((int)pvVar1 + 0x14) & 0xfffffff8;
              uVar15 = 0x4;
            }
          }
        }
        memory = (undefined4 *)lVar3;
        memory[0x2] = 0x2ee620;
        *memory = 0xffffff;
        memory[0x4] = uVar15;
        memory[0x5] = uVar10;
        memory[0x7] = 0x3f800000;
        *(undefined *)((int)memory + 0x22) = 0xff;
        memory[0x1] = 0x0;
        *(undefined *)(memory + 0x3) = 0xff;
      }
      memory[0x6] = 0x3f800000;
      *(undefined *)(memory + 0x8) = 0xff;
      *(undefined *)((int)memory + 0x21) = 0xff;
      if (true) {
        return lVar3;
      }
      memory = (undefined4 *)MemoryAllocate(0x10);
      lVar3 = (long)(int)memory;
      *(undefined *)(memory + 0x3) = 0xff;
      *memory = 0xffffff;
      memory[0x2] = 0x2eee88;
      uVar10 = memory[0x3] & 0xfffffeff;
      if (false) {
        memory = (undefined4 *)MemoryAllocate(0x10);
        lVar3 = (long)(int)memory;
        puVar5 = &DAT_002efcc8;
        if (true) goto LAB_00106c0c;
        memory = (undefined4 *)MemoryAllocate(0xc);
        lVar3 = (long)(int)memory;
        puVar5 = &DAT_002efc88;
        if (true) goto LAB_00106c0c;
        memory = (undefined4 *)MemoryAllocate(0x10);
        lVar3 = (long)(int)memory;
        *(undefined2 *)(memory + 0x3) = 0xffff;
        *memory = 0xffffff;
        uVar10 = memory[0x3] & 0xfffcffff;
        memory[0x2] = 0x2efc48;
        if (true) goto LAB_00106430;
        memory = (undefined4 *)MemoryAllocate(0x14);
        *memory = 0xffffff;
        memory[0x2] = 0x2efd48;
        memory[0x4] = memory[0x4] & 0xfffffffc;
        goto LAB_0010565c;
      }
LAB_001067d0:
      memory[0x3] = uVar10;
      memory[0x1] = 0x0;
      if (true) {
        return lVar3;
      }
      memory = (undefined4 *)MemoryAllocate(0xc);
      lVar3 = (long)(int)memory;
      puVar5 = &DAT_002f1b70;
      if (true) goto LAB_00106c0c;
      memory = (undefined4 *)MemoryAllocate(0x94);
      lVar3 = (long)(int)memory;
      memory[0x2] = 0x2f19f0;
      *memory = 0xffffff;
      lVar8 = (long)(int)(memory + 0x3);
      memory[0x1] = 0x0;
      lVar11 = 0x0;
      lVar13 = 0x1;
LAB_0010682c:
      FUN_0023f2c0(lVar8,lVar11,lVar13);
      if (true) {
        return lVar3;
      }
      memory = (undefined4 *)MemoryAllocate(0xc);
      lVar3 = (long)(int)memory;
      puVar5 = &DAT_002ef888;
      if (true) goto LAB_00106c0c;
      memory = (undefined4 *)MemoryAllocate(0x10);
      puVar5 = &DAT_002ee6c8;
LAB_00106870:
      *(undefined **)(memory + 0x2) = puVar5;
      *memory = 0xffffff;
      memory[0x1] = 0x0;
      *(undefined2 *)(memory + 0x3) = 0x0;
      if (true) {
        return (long)(int)memory;
      }
      memory = (undefined4 *)MemoryAllocate(0x18);
      memory[0x2] = 0x2ef608;
      memory[0x3] = 0xffffffff;
      *memory = 0xffffff;
      memory[0x1] = 0x0;
      FUN_00209900(memory + 0x4,0x0);
      memory[0x5] = 0x3f800000;
      if (true) {
        return (long)(int)memory;
      }
      memory = (undefined4 *)MemoryAllocate(0x10);
      lVar3 = (long)(int)memory;
      puVar5 = &DAT_002ef5c8;
      if (false) {
        memory = (undefined4 *)MemoryAllocate(0xc);
        lVar3 = (long)(int)memory;
        puVar5 = &DAT_002ef588;
        if (true) goto LAB_00106c0c;
        memory = (undefined4 *)MemoryAllocate(0x18);
        lVar3 = (long)(int)memory;
        *memory = 0xffffff;
        memory[0x2] = 0x2ef548;
        memory[0x3] = 0xffffffff;
        memory[0x1] = 0x0;
        FUN_002098d8(0x0,(long)(int)(memory + 0x4));
        piVar7 = memory + 0x5;
        goto LAB_00106958;
      }
      goto LAB_00106a0c;
    }
LAB_00104718:
    memory[0x2] = 0x2fe6d8;
    *memory = 0xffffff;
    memory[0x5] = uVar10 & 0xff0ffff0 | uVar12;
    memory[0x1] = 0x0;
    memory[0x3] = 0x0;
    memory[0x4] = 0x0;
    if (true) {
      return (long)(int)memory;
    }
    memory = (undefined4 *)MemoryAllocate(0xc);
    lVar3 = (long)(int)memory;
    puVar5 = &DAT_002fe158;
    if (true) goto LAB_00106c0c;
    memory = (undefined4 *)MemoryAllocate(0x10);
    lVar3 = (long)(int)memory;
    *(undefined2 *)(memory + 0x3) = 0xffff;
    *memory = 0xffffff;
    uVar10 = memory[0x3] & 0xfffeffff;
    memory[0x2] = 0x3019b8;
    if (true) goto LAB_00106430;
    memory = (undefined4 *)MemoryAllocate(0xc0);
    memory[0x2] = 0x301938;
    *memory = 0xffffff;
    memory[0x1] = 0x0;
    memory[0x4] = 0x0;
    memory[0x7] = 0x3f800000;
    memory[0x5] = 0x0;
    memory[0x6] = 0x0;
    FUN_0023f2c0((long)(int)(memory + 0x8),0x1,0x0);
    *(undefined *)(memory + 0x2a) = 0xff;
    *(ulong *)(memory + 0x2a) = *(ulong *)(memory + 0x2a) & 0xffffff00fffff0ff | 0xffff000;
    FUN_002098d8(0x3f800000,(long)(int)(memory + 0x2c));
    memory[0x2d] = 0x0;
    memory[0x2e] = 0x0;
    memory[0x2f] = 0x0;
    if (true) {
      return (long)(int)memory;
    }
    pvVar1 = MemoryAllocate(0x10);
    lVar3 = (long)(int)pvVar1;
    puVar5 = &DAT_00301af8;
    uVar10 = *(uint *)((int)pvVar1 + 0xc) & 0xfffffff8 | 0x2;
    if (false) {
      memory = (undefined4 *)MemoryAllocate(0x30);
      lVar3 = (long)(int)memory;
      memory[0x2] = 0x301ab8;
      uVar9 = 0xff7fffffffffffff;
      *memory = 0xffffff;
      puVar6 = memory + 0x4;
      memory[0x1] = 0x0;
      uVar4 = *(ulong *)(memory + 0x2) & 0xff9e07f8ffffffff | 0x1e07fa00000000;
      goto LAB_00104a64;
    }
LAB_001060a8:
    memory = (undefined4 *)lVar3;
    *memory = 0xffffff;
    *(undefined **)(memory + 0x2) = puVar5;
    memory[0x3] = uVar10;
    memory[0x1] = 0x0;
    if (true) {
      return lVar3;
    }
    memory = (undefined4 *)MemoryAllocate(0x14);
    *memory = 0xffffff;
    memory[0x2] = 0x2eea08;
    memory[0x3] = memory[0x3] & 0xffffff00;
    memory[0x1] = 0x0;
    memory[0x4] = 0x0;
    if (true) {
      return (long)(int)memory;
    }
    memory = (undefined4 *)MemoryAllocate(0x18);
    *memory = 0xffffff;
    memory[0x2] = 0x2ee9c8;
    memory[0x4] = memory[0x4] & 0xfffffffe | 0x2;
    *(undefined *)((int)memory + 0x15) = 0xff;
    memory[0x1] = 0x0;
    *(undefined *)(memory + 0x3) = 0xff;
    *(undefined *)((int)memory + 0xd) = 0xff;
    *(undefined *)((int)memory + 0xe) = 0xff;
    *(undefined *)((int)memory + 0xf) = 0xff;
    *(undefined *)(memory + 0x5) = 0xff;
    if (true) {
      return (long)(int)memory;
    }
    memory = (undefined4 *)MemoryAllocate(0xc);
    lVar3 = (long)(int)memory;
    puVar5 = &DAT_002eeb08;
    if (true) goto LAB_00106c0c;
    memory = (undefined4 *)MemoryAllocate(0xc);
    lVar3 = (long)(int)memory;
    puVar5 = &DAT_002eeac8;
    if (true) goto LAB_00106c0c;
    memory = (undefined4 *)MemoryAllocate(0xc);
    lVar3 = (long)(int)memory;
    puVar5 = &DAT_002ee988;
    if (true) goto LAB_00106c0c;
    memory = (undefined4 *)MemoryAllocate(0xc);
    lVar3 = (long)(int)memory;
    puVar5 = &DAT_002ee948;
    if (true) goto LAB_00106c0c;
    memory = (undefined4 *)MemoryAllocate(0xc);
    lVar3 = (long)(int)memory;
    puVar5 = &DAT_002ee8c8;
    if (true) goto LAB_00106c0c;
    memory = (undefined4 *)MemoryAllocate(0xc);
    lVar3 = (long)(int)memory;
    puVar5 = &DAT_002ee888;
    if (true) goto LAB_00106c0c;
    memory = (undefined4 *)MemoryAllocate(0xc);
    lVar3 = (long)(int)memory;
    puVar5 = &DAT_002ee848;
    if (true) goto LAB_00106c0c;
    memory = (undefined4 *)MemoryAllocate(0x14);
    lVar3 = (long)(int)memory;
    *memory = 0xffffff;
    memory[0x2] = 0x2ee808;
    memory[0x3] = memory[0x3] & 0xfffffff8;
LAB_00106278:
    memory[0x1] = 0x0;
    memory[0x4] = 0x0;
    if (true) {
      return lVar3;
    }
    memory = (undefined4 *)MemoryAllocate(0x18);
    *memory = 0xffffff;
    memory[0x2] = 0x2ee7c8;
    memory[0x3] = memory[0x3] & 0xfffffff8;
    memory[0x1] = 0x0;
    memory[0x4] = 0x0;
    memory[0x5] = 0x0;
    if (true) {
      return (long)(int)memory;
    }
    memory = (undefined4 *)MemoryAllocate(0xc);
    lVar3 = (long)(int)memory;
    puVar5 = &DAT_002ee788;
    if (true) goto LAB_00106c0c;
    memory = (undefined4 *)MemoryAllocate(0xc);
    lVar3 = (long)(int)memory;
    puVar5 = &DAT_002eeb88;
    if (true) goto LAB_00106c0c;
    memory = (undefined4 *)MemoryAllocate(0xc);
    lVar3 = (long)(int)memory;
    puVar5 = &DAT_002f1db0;
    if (true) goto LAB_00106c0c;
    memory = (undefined4 *)MemoryAllocate(0xc);
    lVar3 = (long)(int)memory;
    puVar5 = &DAT_002f1d70;
    if (true) goto LAB_00106c0c;
    memory = (undefined4 *)MemoryAllocate(0x10);
    lVar3 = (long)(int)memory;
    uVar10 = memory[0x3] & 0xfffffffe;
    *memory = 0xffffff;
    memory[0x2] = 0x2f1d30;
    if (false) {
      memory = (undefined4 *)MemoryAllocate(0xc);
      lVar3 = (long)(int)memory;
      puVar5 = &DAT_002f1cf0;
      if (true) goto LAB_00106c0c;
      memory = (undefined4 *)MemoryAllocate(0xc);
      lVar3 = (long)(int)memory;
      puVar5 = &DAT_002eff08;
      if (true) goto LAB_00106c0c;
      memory = (undefined4 *)MemoryAllocate(0x10);
      lVar3 = (long)(int)memory;
      puVar5 = &DAT_002efec8;
      uVar10 = 0xffffffff;
      if (false) {
        memory = (undefined4 *)MemoryAllocate(0xc);
        lVar3 = (long)(int)memory;
        puVar5 = &DAT_002efe88;
        if (true) goto LAB_00106c0c;
        memory = (undefined4 *)MemoryAllocate(0xc);
        lVar3 = (long)(int)memory;
        puVar5 = &DAT_002efe48;
        if (true) goto LAB_00106c0c;
        memory = (undefined4 *)MemoryAllocate(0x10);
        puVar5 = &DAT_002efe08;
        goto LAB_00106420;
      }
LAB_00106428:
      *memory = 0xffffff;
      *(undefined **)(memory + 0x2) = puVar5;
    }
LAB_00106430:
    memory[0x3] = uVar10;
    memory[0x1] = 0x0;
    if (true) {
      return lVar3;
    }
    pvVar1 = MemoryAllocate(0x1c);
    uVar2 = FUN_001292c0((long)(int)pvVar1);
    if (true) {
      return uVar2;
    }
    memory = (undefined4 *)MemoryAllocate(0x24);
    lVar3 = (long)(int)memory;
    memory[0x5] = 0x0;
    uVar10 = memory[0x5];
    lVar8 = (long)(int)(memory + 0x8);
    *memory = 0xffffff;
    memory[0x2] = 0x2f1c70;
    memory[0x3] = memory[0x3] & 0xffffff00 | 0x1;
    memory[0x4] = 0x41200000;
    memory[0x6] = 0x3f800000;
    memory[0x1] = 0x0;
    memory[0x7] = 0x0;
    if (false) {
      memory = (undefined4 *)MemoryAllocate(0x30);
      *memory = 0xffffff;
      memory[0x2] = 0x2f1c30;
      memory[0x1] = 0x0;
      memory[0x4] = 0x0;
      memory[0x7] = 0x3f800000;
      memory[0x5] = 0x0;
      memory[0x6] = 0x0;
      SetUndefinedID?((short *)(memory + 0x8));
      *(undefined2 *)((int)memory + 0x22) = 0xffff;
      *(undefined *)(memory + 0x9) = 0xff;
      memory[0xa] = 0x0;
      *(ulong *)(memory + 0x8) = *(ulong *)(memory + 0x8) & 0xffff10ffffffffff | 0x120000000000;
      if (true) {
        return (long)(int)memory;
      }
      pvVar1 = MemoryAllocate(0x50);
      uVar2 = FUN_0011ccd0((long)(int)pvVar1);
      if (true) {
        return uVar2;
      }
      memory = (undefined4 *)MemoryAllocate(0x18);
      memory[0x2] = 0x2ef808;
      *memory = 0xffffff;
      memory[0x3] = memory[0x3] & 0xffffff80;
      memory[0x5] = 0x3f800000;
      memory[0x1] = 0x0;
      memory[0x4] = 0x3f800000;
      if (true) {
        return (long)(int)memory;
      }
      memory = (undefined4 *)MemoryAllocate(0x10);
      puVar5 = &DAT_002efd08;
      if (false) {
        memory = (undefined4 *)MemoryAllocate(0xc);
        lVar3 = (long)(int)memory;
        puVar5 = &DAT_002ef148;
        if (true) goto LAB_00106c0c;
        memory = (undefined4 *)MemoryAllocate(0xc);
        lVar3 = (long)(int)memory;
        puVar5 = &DAT_002ef108;
        if (true) goto LAB_00106c0c;
        memory = (undefined4 *)MemoryAllocate(0x2c);
        *memory = 0xffffff;
        memory[0x2] = 0x2f1bf0;
        memory[0x3] = 0x41200000;
        memory[0x7] = 0x42700000;
        *(undefined2 *)(memory + 0xa) = 0xffff;
        memory[0x1] = 0x0;
        memory[0x4] = 0x0;
        memory[0x5] = 0x0;
        memory[0x6] = 0x0;
        *(undefined2 *)(memory + 0x8) = 0xffff;
        *(undefined2 *)((int)memory + 0x22) = 0xffff;
        *(undefined2 *)(memory + 0x9) = 0xffff;
        *(undefined2 *)((int)memory + 0x26) = 0xffff;
        if (true) {
          return (long)(int)memory;
        }
        memory = (undefined4 *)MemoryAllocate(0x10);
        lVar3 = (long)(int)memory;
        puVar5 = &DAT_002f1ab0;
        if (true) goto LAB_00106a0c;
        memory = (undefined4 *)MemoryAllocate(0x10);
        puVar5 = &DAT_002f1bb0;
      }
      *(undefined **)(memory + 0x2) = puVar5;
      *memory = 0xffffff;
      memory[0x1] = 0x0;
      *(undefined *)(memory + 0x3) = 0x0;
      *(undefined *)((int)memory + 0xd) = 0x0;
      if (true) {
        return (long)(int)memory;
      }
      memory = (undefined4 *)MemoryAllocate(0x18);
      memory[0x2] = 0x2f1b30;
      *memory = 0xffffff;
      memory[0x1] = 0x0;
      *(undefined *)(memory + 0x3) = 0x0;
      *(undefined *)((int)memory + 0xd) = 0x0;
LAB_0010678c:
      memory[0x4] = 0x0;
      memory[0x5] = 0x0;
      if (true) {
        return (long)(int)memory;
      }
      memory = (undefined4 *)MemoryAllocate(0x10);
      lVar3 = (long)(int)memory;
      *(undefined *)(memory + 0x3) = 0x0;
      *memory = 0xffffff;
      uVar10 = memory[0x3] & 0xfffffeff;
      memory[0x2] = 0x2f1af0;
      goto LAB_001067d0;
    }
  }
LAB_00106b80:
  FUN_002098d8(uVar10,lVar8);
  if (true) {
    return lVar3;
  }
  memory = (undefined4 *)MemoryAllocate(0xc);
  lVar3 = (long)(int)memory;
  puVar5 = &DAT_002f1970;
  if (false) {
    memory = (undefined4 *)MemoryAllocate(0x14);
    *memory = 0xffffff;
    memory[0x2] = 0x2ef288;
    memory[0x1] = 0x0;
    FUN_00209900(memory + 0x3,0x0);
    memory[0x4] = 0x3e800000;
    if (true) {
      return (long)(int)memory;
    }
    memory = (undefined4 *)MemoryAllocate(0x10);
    lVar3 = (long)(int)memory;
    puVar5 = &DAT_002f18f0;
  }
LAB_00106c0c:
  *(undefined **)(memory + 0x2) = puVar5;
  *memory = 0xffffff;
  memory[0x1] = 0x0;
  if (false) {
    lVar3 = 0x0;
  }
  return lVar3;
}

